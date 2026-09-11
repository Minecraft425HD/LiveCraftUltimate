#include "lcu/jobs/job_system.h"

#include <atomic>
#include <mutex>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

using lcu::jobs::JobPriority;
using lcu::jobs::JobState;
using lcu::jobs::JobSystem;

TEST(JobSystem, SubmitAndWaitRunsTheJob) {
    JobSystem jobs(2);
    std::atomic<bool> ran{false};

    const auto handle = jobs.submit([&ran] { ran = true; });
    jobs.wait(handle);

    EXPECT_TRUE(ran.load());
    EXPECT_EQ(jobs.state_of(handle), JobState::Complete);
    EXPECT_TRUE(jobs.is_finished(handle));
}

TEST(JobSystem, JobRunsOnAWorkerThreadNotTheCaller) {
    JobSystem jobs(2);
    const std::thread::id main_id = std::this_thread::get_id();
    std::thread::id job_thread_id;

    const auto handle = jobs.submit([&job_thread_id] { job_thread_id = std::this_thread::get_id(); });
    jobs.wait(handle);

    EXPECT_NE(job_thread_id, main_id);
}

TEST(JobSystem, MultipleIndependentJobsAllComplete) {
    JobSystem jobs(4);
    constexpr int kJobCount = 200;
    std::atomic<int> counter{0};

    for (int i = 0; i < kJobCount; ++i) {
        jobs.submit([&counter] { counter.fetch_add(1, std::memory_order_relaxed); });
    }
    jobs.wait_idle();

    EXPECT_EQ(counter.load(), kJobCount);
}

TEST(JobSystem, DependentJobRunsAfterItsDependency) {
    JobSystem jobs(4);
    std::mutex order_mutex;
    std::vector<char> order;

    const auto a = jobs.submit([&] {
        std::lock_guard<std::mutex> lock(order_mutex);
        order.push_back('A');
    });
    const auto b = jobs.submit(
        [&] {
            std::lock_guard<std::mutex> lock(order_mutex);
            order.push_back('B');
        },
        JobPriority::Normal, {a});

    jobs.wait(b);

    ASSERT_EQ(order.size(), 2u);
    EXPECT_EQ(order[0], 'A');
    EXPECT_EQ(order[1], 'B');
}

TEST(JobSystem, DiamondDependencyRunsFinalJobOnlyAfterBothBranches) {
    JobSystem jobs(4);
    std::atomic<bool> b_done{false};
    std::atomic<bool> c_done{false};
    std::atomic<bool> d_saw_both{false};

    const auto a = jobs.submit([] {});
    const auto b = jobs.submit([&] { b_done = true; }, JobPriority::Normal, {a});
    const auto c = jobs.submit([&] { c_done = true; }, JobPriority::Normal, {a});
    const auto d = jobs.submit([&] { d_saw_both = b_done.load() && c_done.load(); }, JobPriority::Normal,
                                {b, c});

    jobs.wait(d);

    EXPECT_TRUE(d_saw_both.load());
}

TEST(JobSystem, CancelPendingJobPreventsItFromRunning) {
    JobSystem jobs(2);
    std::atomic<bool> gate_open{false};
    std::atomic<bool> dependent_ran{false};

    const auto blocker = jobs.submit([&gate_open] {
        while (!gate_open.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }
    });
    const auto dependent =
        jobs.submit([&dependent_ran] { dependent_ran = true; }, JobPriority::Normal, {blocker});

    // `dependent` is guaranteed still Pending here: `blocker` cannot have
    // completed yet (it's spinning on gate_open), so cancel must succeed.
    const bool cancelled = jobs.cancel(dependent);
    EXPECT_TRUE(cancelled);

    gate_open = true;
    jobs.wait_idle();

    EXPECT_FALSE(dependent_ran.load());
    EXPECT_EQ(jobs.state_of(blocker), JobState::Complete);
    EXPECT_EQ(jobs.state_of(dependent), JobState::Cancelled);
}

TEST(JobSystem, CancellingAJobCascadesToItsDependents) {
    JobSystem jobs(2);
    std::atomic<bool> gate_open{false};
    std::atomic<bool> b_ran{false};

    const auto a = jobs.submit([&gate_open] {
        while (!gate_open.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }
    });
    const auto b = jobs.submit([&b_ran] { b_ran = true; }, JobPriority::Normal, {a});

    // `a` is still spinning, so `b` is guaranteed Pending - cancel `a`
    // itself (also still Pending, since it hasn't been picked up as
    // Ready... actually `a` has no dependencies so it becomes Ready
    // immediately; cancel() racing against a worker picking it up is
    // possible, so only assert the cascade when cancellation succeeds).
    const bool cancelled = jobs.cancel(a);
    gate_open = true;
    jobs.wait_idle();

    if (cancelled) {
        EXPECT_EQ(jobs.state_of(a), JobState::Cancelled);
        EXPECT_EQ(jobs.state_of(b), JobState::Cancelled);
        EXPECT_FALSE(b_ran.load());
    } else {
        // `a` had already started running before cancel() took the lock -
        // legitimate race, not a bug. `a` must have completed normally and
        // `b` must have been allowed to run.
        EXPECT_EQ(jobs.state_of(a), JobState::Complete);
        EXPECT_EQ(jobs.state_of(b), JobState::Complete);
        EXPECT_TRUE(b_ran.load());
    }
}

TEST(JobSystem, CancellingAlreadyCompleteJobFails) {
    JobSystem jobs(2);
    const auto handle = jobs.submit([] {});
    jobs.wait(handle);

    EXPECT_FALSE(jobs.cancel(handle));
    EXPECT_EQ(jobs.state_of(handle), JobState::Complete);
}

TEST(JobSystem, CancellingUnknownHandleFails) {
    JobSystem jobs(2);
    EXPECT_FALSE(jobs.cancel(999999));
}

TEST(JobSystem, HigherPriorityJobsRunFirstWhenQueuedTogether) {
    // Single worker so scheduling order is deterministic: a "blocker" job
    // occupies the sole worker while several differently-prioritized jobs
    // queue up behind it, then we release the blocker and observe order.
    JobSystem jobs(1);
    std::atomic<bool> gate_open{false};
    std::mutex order_mutex;
    std::vector<JobPriority> order;

    const auto blocker = jobs.submit([&gate_open] {
        while (!gate_open.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }
    });

    auto record = [&](JobPriority priority) {
        return [&order_mutex, &order, priority] {
            std::lock_guard<std::mutex> lock(order_mutex);
            order.push_back(priority);
        };
    };

    // Submitted low-to-high on purpose, so passing requires the priority
    // queue to actually reorder them, not just preserve submission order.
    jobs.submit(record(JobPriority::Low), JobPriority::Low, {blocker});
    jobs.submit(record(JobPriority::Normal), JobPriority::Normal, {blocker});
    jobs.submit(record(JobPriority::High), JobPriority::High, {blocker});
    jobs.submit(record(JobPriority::Critical), JobPriority::Critical, {blocker});

    // Give the dependent jobs a moment to actually land in the ready
    // queue behind the still-blocked worker before releasing it.
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    gate_open = true;
    jobs.wait_idle();

    ASSERT_EQ(order.size(), 4u);
    EXPECT_EQ(order[0], JobPriority::Critical);
    EXPECT_EQ(order[1], JobPriority::High);
    EXPECT_EQ(order[2], JobPriority::Normal);
    EXPECT_EQ(order[3], JobPriority::Low);
}

TEST(JobSystem, WorkerCountMatchesRequestedCount) {
    JobSystem jobs(4);
    EXPECT_EQ(jobs.worker_count(), 4u);
}

TEST(JobSystem, AutoWorkerCountIsAtLeastOne) {
    JobSystem jobs(0);
    EXPECT_GE(jobs.worker_count(), 1u);
}

TEST(JobSystem, UnfinishedJobCountStartsAtZero) {
    JobSystem jobs(2);
    EXPECT_EQ(jobs.unfinished_job_count(), 0u);
}

TEST(JobSystem, UnfinishedJobCountReflectsAJobInFlight) {
    JobSystem jobs(2);
    std::atomic<bool> gate_open{false};

    const auto blocker = jobs.submit([&gate_open] {
        while (!gate_open.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }
    });

    // A brief spin-wait for the worker to actually pick up `blocker` -
    // submit() returns immediately, before any worker has necessarily
    // dequeued it yet.
    while (jobs.state_of(blocker) != JobState::Running) {
        std::this_thread::yield();
    }
    EXPECT_EQ(jobs.unfinished_job_count(), 1u);

    gate_open = true;
    jobs.wait(blocker);
    EXPECT_EQ(jobs.unfinished_job_count(), 0u);
}

TEST(JobSystem, UnfinishedJobCountCountsMultiplePendingJobs) {
    JobSystem jobs(1);  // single worker: guarantees both jobs stay queued together
    std::atomic<bool> gate_open{false};

    const auto blocker = jobs.submit([&gate_open] {
        while (!gate_open.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }
    });
    const auto second = jobs.submit([] {});

    while (jobs.state_of(blocker) != JobState::Running) {
        std::this_thread::yield();
    }
    // `second` can't have run yet (single worker, still busy on
    // `blocker`) - both count as unfinished.
    EXPECT_EQ(jobs.unfinished_job_count(), 2u);

    gate_open = true;
    jobs.wait(second);
    EXPECT_EQ(jobs.unfinished_job_count(), 0u);
}
