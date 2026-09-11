#pragma once

#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

#include "lcu/core/types.h"

namespace lcu::jobs {

using JobHandle = u64;
constexpr JobHandle kInvalidJobHandle = 0;

enum class JobPriority : u8 {
    Low,
    Normal,
    High,
    Critical,
};

enum class JobState : u8 {
    Pending,    // waiting on one or more unfinished dependencies
    Ready,      // all dependencies satisfied, waiting for a free worker
    Running,
    Complete,
    Cancelled,
};

// Minimal worker-thread job system: priority, dependencies, cancellation
// (brief section 19). Intended consumers - chunk generation, meshing,
// lighting, serialization, compression, asset loading, world saving -
// don't exist yet (Phase 2/3), so this is deliberately generic rather
// than shaped around one specific workload.
//
// Correctness-first design: one mutex + condition variable guards all
// scheduling state. Job bodies (the submitted std::function) run
// unlocked, so only bookkeeping is serialized, not job work itself. This
// is not the fastest possible job system, but it is a correct starting
// point - see DECISIONS.md. Revisit only if profiling (Phase 11) shows
// scheduling overhead actually matters; don't optimize ahead of a
// measurement (brief section 76).
class JobSystem : public NonCopyable {
   public:
    // worker_count == 0 selects hardware_concurrency() - 1 workers
    // (leaving one core for the caller), clamped to at least 1.
    explicit JobSystem(u32 worker_count = 0);
    ~JobSystem();

    JobSystem(JobSystem&&) = delete;
    JobSystem& operator=(JobSystem&&) = delete;

    // Submits a job that becomes Ready once every handle in `dependencies`
    // has reached Complete (immediately Ready if `dependencies` is empty).
    // If any dependency is already Cancelled (or becomes cancelled before
    // this job runs), this job is cancelled too rather than run - a
    // dependency that will never complete means this job's precondition
    // can never be satisfied. An unrecognized dependency handle is
    // treated as already satisfied (not an error), so a handle from an
    // already-drained job system doesn't deadlock a new submission.
    JobHandle submit(std::function<void()> fn, JobPriority priority = JobPriority::Normal,
                      const std::vector<JobHandle>& dependencies = {});

    // Prevents a Pending/Ready job - and, recursively, everything that
    // (transitively) depends on it - from ever running. Returns false and
    // does nothing if the job is already Running, Complete, or Cancelled:
    // this cancels jobs that haven't started, it does not preempt a job
    // already in flight.
    bool cancel(JobHandle handle);

    JobState state_of(JobHandle handle) const;
    bool is_finished(JobHandle handle) const;  // Complete or Cancelled

    // Blocks the calling thread until `handle` reaches Complete or
    // Cancelled (or returns immediately if the handle is unrecognized).
    // For tests and explicit synchronization points, not a per-frame hot
    // path.
    void wait(JobHandle handle);

    // Blocks until every job submitted so far has finished (Complete or
    // Cancelled). Mainly for tests and shutdown/sync barriers.
    void wait_idle();

    u32 worker_count() const { return static_cast<u32>(workers_.size()); }

    // Jobs submitted but not yet Complete/Cancelled right now (Phase 36,
    // brief section 60's debug overlay "jobs" stat) - a real, if
    // usually near-zero, number for this codebase's current usage
    // pattern (every call site submits then immediately waits, so a
    // job is rarely in flight long enough to observe here), rather
    // than a fake/hardcoded placeholder for a stat this system doesn't
    // otherwise track anywhere.
    u64 unfinished_job_count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return unfinished_count_;
    }

   private:
    struct JobRecord {
        JobPriority priority = JobPriority::Normal;
        std::function<void()> fn;
        std::vector<JobHandle> dependents;
        u32 pending_dependency_count = 0;
        JobState state = JobState::Pending;
    };

    void worker_loop();
    // The following require mutex_ to already be held by the caller.
    void mark_finished_locked(JobHandle handle, JobState final_state);
    void cancel_locked(JobHandle handle);
    JobHandle pop_highest_priority_ready_locked();

    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::vector<std::thread> workers_;
    std::unordered_map<JobHandle, JobRecord> jobs_;
    std::vector<JobHandle> ready_queue_;
    JobHandle next_handle_ = 1;
    u64 unfinished_count_ = 0;
    bool shutting_down_ = false;
};

}  // namespace lcu::jobs
