#include "lcu/jobs/job_system.h"

#include <algorithm>

#include "lcu/core/assert.h"

namespace lcu::jobs {

namespace {

u32 default_worker_count() {
    const unsigned hw = std::thread::hardware_concurrency();
    if (hw <= 1) {
        return 1;
    }
    return hw - 1;
}

}  // namespace

JobSystem::JobSystem(u32 worker_count) {
    const u32 count = worker_count != 0 ? worker_count : default_worker_count();
    workers_.reserve(count);
    for (u32 i = 0; i < count; ++i) {
        workers_.emplace_back([this] { worker_loop(); });
    }
}

JobSystem::~JobSystem() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        shutting_down_ = true;
    }
    cv_.notify_all();
    for (std::thread& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

JobHandle JobSystem::submit(std::function<void()> fn, JobPriority priority,
                             const std::vector<JobHandle>& dependencies) {
    std::lock_guard<std::mutex> lock(mutex_);

    const JobHandle handle = next_handle_++;

    JobRecord record;
    record.priority = priority;
    record.fn = std::move(fn);

    bool cancelled_by_dependency = false;
    u32 pending = 0;
    for (JobHandle dep : dependencies) {
        auto it = jobs_.find(dep);
        if (it == jobs_.end()) {
            continue;  // unrecognized handle: treat as already satisfied
        }
        if (it->second.state == JobState::Cancelled) {
            cancelled_by_dependency = true;
        } else if (it->second.state != JobState::Complete) {
            ++pending;
            it->second.dependents.push_back(handle);
        }
    }

    record.pending_dependency_count = pending;
    record.state = JobState::Pending;
    ++unfinished_count_;
    jobs_.emplace(handle, std::move(record));

    if (cancelled_by_dependency) {
        cancel_locked(handle);
    } else if (pending == 0) {
        jobs_[handle].state = JobState::Ready;
        ready_queue_.push_back(handle);
    }

    cv_.notify_all();
    return handle;
}

bool JobSystem::cancel(JobHandle handle) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = jobs_.find(handle);
    if (it == jobs_.end()) {
        return false;
    }
    if (it->second.state == JobState::Running || it->second.state == JobState::Complete ||
        it->second.state == JobState::Cancelled) {
        return false;
    }
    cancel_locked(handle);
    cv_.notify_all();
    return true;
}

void JobSystem::cancel_locked(JobHandle handle) {
    auto it = jobs_.find(handle);
    if (it == jobs_.end()) {
        return;
    }
    if (it->second.state == JobState::Running || it->second.state == JobState::Complete ||
        it->second.state == JobState::Cancelled) {
        return;
    }
    if (it->second.state == JobState::Ready) {
        auto queue_it = std::find(ready_queue_.begin(), ready_queue_.end(), handle);
        if (queue_it != ready_queue_.end()) {
            ready_queue_.erase(queue_it);
        }
    }
    mark_finished_locked(handle, JobState::Cancelled);
}

void JobSystem::mark_finished_locked(JobHandle handle, JobState final_state) {
    auto it = jobs_.find(handle);
    if (it == jobs_.end()) {
        return;
    }
    if (it->second.state == JobState::Complete || it->second.state == JobState::Cancelled) {
        return;  // already finished - guards against double-processing in diamond dependency graphs
    }

    it->second.state = final_state;
    --unfinished_count_;

    // Copied rather than referenced: the loop below (for Complete) and the
    // recursive cancel_locked calls (for Cancelled) both touch other
    // entries of jobs_, and this keeps the dependents list stable
    // regardless of what those calls do.
    const std::vector<JobHandle> dependents = it->second.dependents;

    if (final_state == JobState::Cancelled) {
        for (JobHandle dependent : dependents) {
            cancel_locked(dependent);
        }
        return;
    }

    for (JobHandle dependent : dependents) {
        auto dep_it = jobs_.find(dependent);
        if (dep_it == jobs_.end() || dep_it->second.state != JobState::Pending) {
            continue;
        }
        LCU_ASSERT(dep_it->second.pending_dependency_count > 0);
        if (--dep_it->second.pending_dependency_count == 0) {
            dep_it->second.state = JobState::Ready;
            ready_queue_.push_back(dependent);
        }
    }
}

JobHandle JobSystem::pop_highest_priority_ready_locked() {
    LCU_ASSERT(!ready_queue_.empty());

    auto best = ready_queue_.begin();
    for (auto it = ready_queue_.begin(); it != ready_queue_.end(); ++it) {
        if (jobs_[*it].priority > jobs_[*best].priority) {
            best = it;
        }
    }
    const JobHandle handle = *best;
    ready_queue_.erase(best);
    return handle;
}

void JobSystem::worker_loop() {
    while (true) {
        std::function<void()> fn;
        JobHandle handle = kInvalidJobHandle;

        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this] { return shutting_down_ || !ready_queue_.empty(); });
            if (shutting_down_ && ready_queue_.empty()) {
                return;
            }

            handle = pop_highest_priority_ready_locked();
            auto it = jobs_.find(handle);
            LCU_ASSERT(it != jobs_.end() && it->second.state == JobState::Ready);
            it->second.state = JobState::Running;
            fn = it->second.fn;
        }

        if (fn) {
            fn();
        }

        {
            std::lock_guard<std::mutex> lock(mutex_);
            mark_finished_locked(handle, JobState::Complete);
        }
        cv_.notify_all();
    }
}

JobState JobSystem::state_of(JobHandle handle) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = jobs_.find(handle);
    LCU_ASSERT(it != jobs_.end());
    return it->second.state;
}

bool JobSystem::is_finished(JobHandle handle) const {
    const JobState state = state_of(handle);
    return state == JobState::Complete || state == JobState::Cancelled;
}

void JobSystem::wait(JobHandle handle) {
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock, [this, handle] {
        auto it = jobs_.find(handle);
        return it == jobs_.end() || it->second.state == JobState::Complete ||
               it->second.state == JobState::Cancelled;
    });
}

void JobSystem::wait_idle() {
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock, [this] { return unfinished_count_ == 0; });
}

}  // namespace lcu::jobs
