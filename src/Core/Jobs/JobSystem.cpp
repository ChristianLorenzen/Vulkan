#include "Core/Jobs/JobSystem.hpp"

#include <algorithm>
#include <cassert>
#include <chrono>

namespace Faye::Jobs
{
    namespace
    {
        // Which worker deque does this thread own? -1 on the main thread and
        // any thread the pool did not create. (File-scope thread_local: fine
        // while there is one JobSystem per process, which is the design.)
        thread_local int tlsWorkerIndex = -1;
    }

    JobSystem::JobSystem(unsigned workerCount)
        : mainThreadId(std::this_thread::get_id())
    {
        jobPool = std::make_unique<Job[]>(kMaxJobs);
        freeSlots.reserve(kMaxJobs);
        for (uint32_t i = kMaxJobs; i-- > 0;)
            freeSlots.push_back(i);

        workerDeques.reserve(workerCount);
        for (unsigned w = 0; w < workerCount; ++w)
            workerDeques.push_back(std::make_unique<WorkDequeue>());

        // Spawn only after every deque exists: a worker may try to steal from
        // any sibling the moment it starts.
        workers.reserve(workerCount);
        for (unsigned w = 0; w < workerCount; ++w)
            workers.emplace_back([this, w] { workerMain(w); });
    }

    JobSystem::~JobSystem()
    {
        stopFlag.store(true, std::memory_order_release);
        workAvailable.notify_all();
        for (std::thread &worker : workers)
            worker.join();
        // Anything still in flight now is a caller bug: the owner must wait()
        // on its jobs and pump the main-thread queue before teardown.
        assert(freeSlots.size() == kMaxJobs && "jobs still in flight at JobSystem destruction");
    }

    uint32_t JobSystem::allocateSlot()
    {
        std::scoped_lock lock(freeSlotsMutex);
        assert(!freeSlots.empty() && "job pool exhausted: raise kMaxJobs or wait() more often");
        const uint32_t slot = freeSlots.back();
        freeSlots.pop_back();
        return slot;
    }

    void JobSystem::recycleSlot(uint32_t jobIndex)
    {
        Job &job = jobPool[jobIndex];
        job.fn = nullptr;
        job.debugName = nullptr;
        // From this increment on, every outstanding handle to the finished job
        // observes a generation mismatch and reads as complete — the ABA guard.
        job.generation.fetch_add(1, std::memory_order_release);
        std::scoped_lock lock(freeSlotsMutex);
        freeSlots.push_back(jobIndex);
    }

    bool JobSystem::isComplete(JobHandle handle) const
    {
        if (handle.isNull())
            return true;
        const Job &job = jobPool[handle.index];
        if (job.generation.load(std::memory_order_acquire) != handle.generation)
            return true;   // slot recycled => the job this handle named finished long ago
        return job.finished.load(std::memory_order_acquire);
    }

    JobHandle JobSystem::schedule(std::function<void()> fn,
                                  std::span<const JobHandle> deps,
                                  const char *debugName)
    {
        return scheduleImpl(std::move(fn), deps, /*mainThreadOnly=*/false, debugName);
    }

    JobHandle JobSystem::scheduleMainThread(std::function<void()> fn,
                                            std::span<const JobHandle> deps,
                                            const char *debugName)
    {
        return scheduleImpl(std::move(fn), deps, /*mainThreadOnly=*/true, debugName);
    }

    JobHandle JobSystem::scheduleImpl(std::function<void()> fn, std::span<const JobHandle> deps,
                                      bool mainThreadOnly, const char *debugName)
    {
        // Single-threaded mode: run inline. Dependencies are trivially
        // satisfied because every previously scheduled job already ran inline.
        if (workers.empty())
        {
            if (fn)
                fn();
            return JobHandle{};   // null == complete
        }

        const uint32_t slot = allocateSlot();
        Job &job = jobPool[slot];
        job.fn = std::move(fn);
        job.mainThreadOnly = mainThreadOnly;
        job.debugName = debugName;
        job.finished.store(false, std::memory_order_relaxed);
        job.unfinished.store(1, std::memory_order_relaxed);
        const JobHandle handle{slot, job.generation.load(std::memory_order_relaxed)};

        // Sentinel idiom: deps.size() + 1. The +1 is our stake; the count
        // cannot hit zero while we are still linking, so a dependency that
        // completes mid-loop can never trigger a second enqueue.
        job.pendingDeps.store(int32_t(deps.size()) + 1, std::memory_order_release);

        for (const JobHandle dep : deps)
            if (!tryAddDependent(dep, handle))   // dep already complete:
                job.pendingDeps.fetch_sub(1, std::memory_order_acq_rel);   // count it ourselves

        // Drop our stake. Whoever brings the count to zero — us, here, or a
        // completing dependency inside finishJob — enqueues. Exactly one
        // thread observes the 1 -> 0 transition.
        if (job.pendingDeps.fetch_sub(1, std::memory_order_acq_rel) == 1)
            enqueue(slot);

        return handle;
    }

    bool JobSystem::tryAddDependent(JobHandle dep, JobHandle dependent)
    {
        if (isComplete(dep))
            return false;   // fast path, no lock
        Job &depJob = jobPool[dep.index];
        std::scoped_lock lock(depJob.depMutex);
        // Re-check under the lock: between the check above and here, dep may
        // have finished and drained its dependents list (finishJob flips
        // `finished` under this same mutex, so "not complete" is stable now).
        if (isComplete(dep))
            return false;
        depJob.dependents.push_back(dependent);
        return true;
    }

    void JobSystem::enqueue(uint32_t jobIndex)
    {
        Job &job = jobPool[jobIndex];
        if (job.mainThreadOnly)
            mainThreadQueue.push(jobIndex);
        else if (tlsWorkerIndex >= 0)
            workerDeques[size_t(tlsWorkerIndex)]->pushBottom(jobIndex);   // own deque: LIFO, cache-hot
        else
            globalQueue.pushBottom(jobIndex);   // main/foreign thread: injection queue
        workAvailable.notify_one();
    }

    void JobSystem::workerMain(unsigned workerIndex)
    {
        tlsWorkerIndex = int(workerIndex);
        while (!stopFlag.load(std::memory_order_acquire))
        {
            uint32_t jobIndex = 0;
            if (tryGetJob(jobIndex))
            {
                executeJob(jobIndex);
            }
            else
            {
                // Nothing anywhere: sleep until schedule() notifies. The 1 ms
                // timeout papers over the lost-wakeup window of this simple
                // sleep protocol; it costs one spurious wake per ms when idle.
                std::unique_lock lock(sleepMutex);
                workAvailable.wait_for(lock, std::chrono::milliseconds(1));
            }
        }
    }

    bool JobSystem::tryGetJob(uint32_t &outJobIndex)
    {
        // 1. Own deque, LIFO end (workers only; the main thread has none).
        if (tlsWorkerIndex >= 0 && workerDeques[size_t(tlsWorkerIndex)]->popBottom(outJobIndex))
            return true;

        // 2. Injection queue: jobs enqueued by non-worker threads.
        if (globalQueue.stealTop(outJobIndex))
            return true;

        // 3. Steal, FIFO end. Rotate the starting victim so idle threads
        //    don't all hammer deque 0; stick with a productive victim.
        const size_t dequeCount = workerDeques.size();
        static thread_local size_t nextVictim = size_t(tlsWorkerIndex + 1);
        for (size_t i = 0; i < dequeCount; ++i)
        {
            const size_t victim = (nextVictim + i) % dequeCount;
            if (int(victim) == tlsWorkerIndex)
                continue;
            if (workerDeques[victim]->stealTop(outJobIndex))
            {
                nextVictim = victim;
                return true;
            }
        }
        return false;
    }

    void JobSystem::executeJob(uint32_t jobIndex)
    {
        Job &job = jobPool[jobIndex];
        if (job.fn)
        {
            // Move the closure out so captured resources are released when the
            // call returns — not when the slot is eventually recycled.
            std::function<void()> fn = std::move(job.fn);
            job.fn = nullptr;
            fn();
        }
        finishJob(jobIndex);
    }

    void JobSystem::finishJob(uint32_t jobIndex)
    {
        Job &job = jobPool[jobIndex];
        if (job.unfinished.fetch_sub(1, std::memory_order_acq_rel) != 1)
            return;   // parallelFor children still outstanding

        std::vector<JobHandle> toNotify;
        {
            // Same lock tryAddDependent takes: once `finished` flips under it,
            // no new dependent can slip into the list we are about to drain.
            std::scoped_lock lock(job.depMutex);
            job.finished.store(true, std::memory_order_release);
            toNotify.swap(job.dependents);
        }
        for (const JobHandle dependent : toNotify)
            if (jobPool[dependent.index].pendingDeps.fetch_sub(1, std::memory_order_acq_rel) == 1)
                enqueue(dependent.index);   // last dependency satisfied -> runnable

        recycleSlot(jobIndex);
    }

    void JobSystem::wait(JobHandle handle)
    {
        if (isComplete(handle))
            return;

        const bool poolParticipant = (tlsWorkerIndex >= 0) || isMainThread();
        while (!isComplete(handle))
        {
            if (poolParticipant)
            {
                if (isMainThread())
                    pumpMainThread();   // the waited job may itself be main-thread-only
                uint32_t jobIndex = 0;
                if (tryGetJob(jobIndex))
                {
                    executeJob(jobIndex);
                    continue;
                }
            }
            std::this_thread::yield();
        }
    }

    void JobSystem::waitAll(std::span<const JobHandle> handles)
    {
        for (const JobHandle handle : handles)
            wait(handle);   // done-ness is monotonic; order is irrelevant
    }

    JobHandle JobSystem::parallelFor(size_t count, size_t grainSize,
                                     std::function<void(size_t, size_t)> body)
    {
        if (count == 0)
            return JobHandle{};
        grainSize = std::max<size_t>(grainSize, 1);
        const size_t numChunks = (count + grainSize - 1) / grainSize;
        if (numChunks == 1 || workers.empty())
        {
            body(0, count);   // inline: cheaper than one job
            return JobHandle{};
        }

        // Parent: an fn-less join node. unfinished = children + our sentinel;
        // the returned handle completes when every chunk has reported in.
        const uint32_t parentSlot = allocateSlot();
        Job &parent = jobPool[parentSlot];
        parent.fn = nullptr;
        parent.mainThreadOnly = false;
        parent.debugName = "parallelFor";
        parent.finished.store(false, std::memory_order_relaxed);
        parent.pendingDeps.store(0, std::memory_order_relaxed);
        parent.unfinished.store(int32_t(numChunks) + 1, std::memory_order_release);
        const JobHandle parentHandle{parentSlot, parent.generation.load(std::memory_order_relaxed)};

        // One shared heap copy of the body instead of numChunks copies.
        auto sharedBody = std::make_shared<std::function<void(size_t, size_t)>>(std::move(body));

        for (size_t chunk = 0; chunk < numChunks; ++chunk)
        {
            const size_t begin = chunk * grainSize;
            const size_t end = std::min(begin + grainSize, count);
            schedule([this, sharedBody, begin, end, parentSlot] {
                (*sharedBody)(begin, end);
                finishJob(parentSlot);   // child reports into the parent counter
            }, {}, "parallelFor chunk");
        }

        finishJob(parentSlot);   // drop the sentinel; the parent can now complete
        return parentHandle;
    }

    void JobSystem::pumpMainThread()
    {
        assert(isMainThread() && "pumpMainThread must run on the thread that built the JobSystem");
        const std::deque<uint32_t> local = mainThreadQueue.takeAll();
        for (const uint32_t jobIndex : local)
            executeJob(jobIndex);
        // Jobs that became runnable during this pump run on the next pump.
    }

    bool JobSystem::isMainThread() const
    {
        return std::this_thread::get_id() == mainThreadId;
    }
}
