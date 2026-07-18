#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <span>
#include <thread>
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
    class JobSystem
    {
    public:
        static constexpr uint32_t kMaxJobs = 4096;

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
        std::mutex sleepMutex;
        std::condition_variable workAvailable;

        std::thread::id mainThreadId;         // captured in the constructor
    };
}
