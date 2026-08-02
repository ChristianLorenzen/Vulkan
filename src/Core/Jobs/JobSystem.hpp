#pragma once

#include <atomic>
#include <condition_variable>
#include <deque>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <span>
#include <thread>
#include <unordered_set>
#include <vector>

#include "Core/Jobs/MainThreadQueue.hpp"
#include "Core/Jobs/WorkDequeue.hpp"

namespace Faye::Jobs
{
    // The main thread participates in the pool (it pumps the main-thread
    // queue, renders, and executes jobs inside wait()), so one worker per core
    // would oversubscribe by one.
    inline unsigned hardwareThreadsMinusOne()
    {
        const unsigned hardwareThreads = std::thread::hardware_concurrency();
        return hardwareThreads > 1 ? hardwareThreads - 1 : 1;
    }

    struct JobHandle
    {
        uint32_t index = 0xFFFFFFFF;   // slot in the job pool; 0xFFFFFFFF = null
        uint32_t generation = 0;       // guards against slot reuse (ABA)
        bool isNull() const { return index == 0xFFFFFFFF; }
    };

    struct Job
    {
        std::function<void()> fn;               // the work; empty for parallelFor parents
        std::atomic<int32_t> pendingDeps{0};    // #incomplete dependencies; runnable at 0
        std::atomic<int32_t> unfinished{1};     // 1 for self; parallelFor parents: children + 1
        std::vector<JobHandle> dependents;      // jobs to notify on completion (guarded by depMutex)
        std::mutex depMutex;                    // closes the link-vs-complete race
        std::atomic<uint32_t> generation{0};    // bumped on recycle
        std::atomic<bool> finished{false};
        // Set when this job's body threw, or when a dependency failed and
        // poisoned it. A poisoned job never runs its body — see executeJob.
        std::atomic<bool> failed{false};
        bool mainThreadOnly = false;            // route to MainThreadQueue when runnable
        const char *debugName = nullptr;
    };

    // Work-stealing thread pool with a DAG scheduler expressed through
    // dependency counters, plus a main-thread queue for thread-confined work
    // (Vulkan, GLFW, ImGui).
    //
    // Rules callers must follow:
    //  - Never hold a lock across wait(): the waiting thread executes other
    //    jobs inline, and one of them may want the same lock.
    //  - parallelFor bodies must be order-independent and touch disjoint data.
    //  - Worker jobs touch CPU data only; Vulkan/GLFW/ImGui work goes through
    //    scheduleMainThread.
    //
    // Failure model: a job body that throws does not take the process down. The
    // exception is caught at the worker boundary, logged with the job's
    // debugName, and the job completes as FAILED so the DAG still unwinds and
    // waiters still return. Failure then poisons the job's dependents — a
    // poisoned job is skipped rather than run, because its inputs were never
    // produced — and poison is transitive down the graph.
    //
    // Poisoning does not depend on link order: a dependent registered after its
    // dependency already failed is poisoned too, via a bounded record of
    // recently-failed jobs (see failedRecord). Serial mode reproduces all of
    // this identically, so FAYE_JOBS_SINGLE_THREADED stays a faithful debugger.
    //
    // Use failureCount() to ask whether a batch was clean; there is no
    // per-handle query, because a slot recycles the moment its job finishes.
    class JobSystem
    {
    public:
        static constexpr uint32_t kMaxJobs = 4096;

        // Ceiling on parallelFor's chunk count. Children are allocated in one
        // burst, so this must leave headroom for jobs already in flight; a
        // quarter of the pool is far more chunks than any core count needs.
        static constexpr size_t kMaxParallelForChunks = kMaxJobs / 4;

        // workerCount == 0 => single-threaded mode: schedule() runs everything
        // inline on the calling thread, so every parallel bug reproduces
        // serially under a debugger.
        explicit JobSystem(unsigned workerCount = hardwareThreadsMinusOne());
        ~JobSystem();   // signals stop, joins workers; asserts no jobs in flight

        JobSystem(const JobSystem &) = delete;
        JobSystem &operator=(const JobSystem &) = delete;

        JobHandle schedule(std::function<void()> fn,
                           std::span<const JobHandle> deps = {},
                           const char *debugName = nullptr);
        void wait(JobHandle handle);   // on a pool thread: drains/steals, never blocks
        void waitAll(std::span<const JobHandle> handles);

        JobHandle parallelFor(size_t count, size_t grainSize,
                              std::function<void(size_t begin, size_t end)> body);

        JobHandle scheduleMainThread(std::function<void()> fn,
                                     std::span<const JobHandle> deps = {},
                                     const char *debugName = nullptr);
        void pumpMainThread();         // called once per frame by the main thread
        bool isMainThread() const;

        bool isComplete(JobHandle handle) const;
        bool isSingleThreaded() const { return workers.empty(); }

        // Monotonic count of jobs that failed (threw, or were poisoned by a
        // failed dependency) over this JobSystem's life. Sample it before and
        // after a batch to learn whether the batch was clean.
        uint64_t failureCount() const { return failures.load(std::memory_order_acquire); }

    private:
        uint32_t allocateSlot();
        void recycleSlot(uint32_t jobIndex);
        JobHandle scheduleImpl(std::function<void()> fn, std::span<const JobHandle> deps,
                               bool mainThreadOnly, const char *debugName);
        bool tryAddDependent(JobHandle dep, JobHandle dependent);
        void enqueue(uint32_t jobIndex);
        void workerMain(unsigned workerIndex);
        bool tryGetJob(uint32_t &outJobIndex);
        void executeJob(uint32_t jobIndex);
        void finishJob(uint32_t jobIndex);
        void markFailed(Job &job, const char *reason);
        void logFailure(const char *debugName, const char *reason);
        bool runInline(std::function<void()> fn, const char *debugName);   // false => threw
        JobHandle scheduleInline(std::function<void()> fn, std::span<const JobHandle> deps,
                                 const char *debugName);

        // Job is not movable (mutex + atomics), and workers hold references
        // into the pool, so storage is a fixed array — not a std::vector.
        std::unique_ptr<Job[]> jobPool;
        std::vector<uint32_t> freeSlots;      // stack of recyclable pool indices
        std::mutex freeSlotsMutex;

        std::vector<std::unique_ptr<WorkDequeue>> workerDeques;  // one per worker
        std::vector<std::thread> workers;
        WorkDequeue globalQueue;              // injection: jobs enqueued by non-workers
        MainThreadQueue mainThreadQueue;

        std::atomic<bool> stopFlag{false};
        std::atomic<uint64_t> failures{0};
        std::mutex sleepMutex;
        std::condition_variable workAvailable;

        std::thread::id mainThreadId;         // captured in the constructor

        // Single-threaded mode only (workers.empty()), so no synchronisation:
        // every access is on the one thread that drives everything. Records the
        // ids of jobs that failed, which is what lets serial mode reproduce the
        // threaded poison-propagation semantics. Bounded by the failure count,
        // not the job count.
        uint32_t serialCounter = 0;
        std::unordered_set<uint32_t> serialFailed;

        // Slots recycle the instant a job finishes, so a handle alone cannot
        // tell "completed fine" from "failed and went away". Without this,
        // whether a failure poisoned a dependent would depend on whether the
        // dependent happened to be linked before the dep finished — a race, and
        // an intermittently-applied safety net is worse than none.
        //
        // So completed-as-failed jobs are recorded here by packed
        // (index, generation) and scheduleImpl consults it for deps that were
        // already complete at link time. Bounded FIFO: a failure evicted after
        // kFailedRecordCapacity later failures no longer poisons a
        // newly-linked dependent, which is bounded staleness rather than a race.
        static constexpr size_t kFailedRecordCapacity = 256;
        mutable std::mutex failedRecordMutex;
        std::unordered_set<uint64_t> failedRecord;
        std::deque<uint64_t> failedRecordOrder;   // eviction order

        static uint64_t packHandle(uint32_t index, uint32_t generation)
        {
            return (uint64_t(index) << 32) | uint64_t(generation);
        }
        void recordFailure(uint32_t index, uint32_t generation);
        bool wasRecordedFailure(JobHandle handle) const;
    };
}
