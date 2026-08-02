#include "Core/Jobs/JobSystem.hpp"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <exception>

#include "Core/Logging/Logger.hpp"
#include "quill/LogMacros.h"

namespace Faye::Jobs
{
    namespace
    {
        // Which worker deque does this thread own? -1 on the main thread and
        // any thread the pool did not create. (File-scope thread_local: fine
        // while there is one JobSystem per process, which is the design.)
        thread_local int tlsWorkerIndex = -1;

        // Enforces the assumption tlsWorkerIndex rests on. Two concurrently-live
        // pools would alias worker indices across each other, so a worker in
        // pool B would pop from pool A's deque. Sequential construction (as the
        // tests do) is fine — this only forbids overlap.
        std::atomic<int> liveInstances{0};
    }

    JobSystem::JobSystem(unsigned workerCount)
        : mainThreadId(std::this_thread::get_id())
    {
        [[maybe_unused]] const int previouslyLive =
            liveInstances.fetch_add(1, std::memory_order_acq_rel);
        assert(previouslyLive == 0 &&
               "two live JobSystems alias worker indices through a process-global "
               "thread_local; construct them sequentially");

#ifdef JOBS_SINGLE_THREADED
        // Build-flag override (FAYE_JOBS_SINGLE_THREADED): force inline
        // execution regardless of the requested count, so every parallel bug
        // reproduces serially under a debugger. schedule()/parallelFor() take
        // the workers.empty() inline paths.
        workerCount = 0;
#endif
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
        liveInstances.fetch_sub(1, std::memory_order_acq_rel);
    }

    uint32_t JobSystem::allocateSlot()
    {
        std::scoped_lock lock(freeSlotsMutex);
        if (freeSlots.empty())
        {
            // Fatal in EVERY build, not just debug. An assert here compiles out
            // under NDEBUG and leaves back()/pop_back() on an empty vector —
            // undefined behaviour that corrupts the pool silently. A hard limit
            // you can see beats one you cannot.
            //
            // Deliberately fprintf and not the logger: quill hands the message
            // to a backend thread, so a crash path that logs would either lose
            // the message or block waiting on a flush. stderr is synchronous.
            // Same reasoning as Ecs::reportAccessViolation.
            std::fprintf(stderr,
                         "FATAL: job pool exhausted (%u slots): raise kMaxJobs or wait() more often\n",
                         kMaxJobs);
            std::abort();
        }
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
        // Serial mode: work ran inline at schedule time, and handle.index is a
        // counter rather than a pool slot — indexing jobPool with it would be
        // out of bounds.
        if (workers.empty())
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
        // satisfied because every previously scheduled job already ran inline,
        // so by the time we get here every dep's outcome is already known.
        //
        // Failure handling mirrors the threaded path exactly — caught, counted,
        // and poisoning dependents — because serial mode exists to reproduce
        // parallel behaviour under a debugger. If the two modes disagreed about
        // whether a dependent runs, serial debugging would lie.
        if (workers.empty())
            return scheduleInline(std::move(fn), deps, debugName);

        const uint32_t slot = allocateSlot();
        Job &job = jobPool[slot];
        job.fn = std::move(fn);
        job.mainThreadOnly = mainThreadOnly;
        job.debugName = debugName;
        job.finished.store(false, std::memory_order_relaxed);
        job.failed.store(false, std::memory_order_relaxed);   // slots are recycled: clear stale poison
        job.unfinished.store(1, std::memory_order_relaxed);
        const JobHandle handle{slot, job.generation.load(std::memory_order_relaxed)};

        // Sentinel idiom: deps.size() + 1. The +1 is our stake; the count
        // cannot hit zero while we are still linking, so a dependency that
        // completes mid-loop can never trigger a second enqueue.
        job.pendingDeps.store(int32_t(deps.size()) + 1, std::memory_order_release);

        for (const JobHandle dep : deps)
            if (!tryAddDependent(dep, handle))   // dep already complete:
            {
                // It finished before we could link, so finishJob will never
                // notify us — including of a failure. Consult the record so a
                // dep that completed-as-failed still poisons us.
                if (wasRecordedFailure(dep))
                    job.failed.store(true, std::memory_order_release);
                job.pendingDeps.fetch_sub(1, std::memory_order_acq_rel);   // count it ourselves
            }

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

        // Move the closure out so captured resources are released when the call
        // returns — not when the slot is eventually recycled. Done before the
        // poison check too, so a skipped job still drops its captures promptly.
        std::function<void()> fn = std::move(job.fn);
        job.fn = nullptr;

        // Poisoned by a failed dependency: the inputs this body expects were
        // never produced, so running it would consume garbage. Skip straight to
        // completion — the DAG still unwinds and waiters still return.
        const bool poisoned = job.failed.load(std::memory_order_acquire);

        if (fn && !poisoned)
        {
            // A worker thread must never let anything escape: an exception
            // reaching workerMain is std::terminate, and because finishJob
            // would be skipped, every waiter on this handle would hang forever.
            try
            {
                fn();
            }
            catch (const std::exception &error)
            {
                markFailed(job, error.what());
            }
            catch (...)
            {
                markFailed(job, "unknown exception");
            }
        }

        finishJob(jobIndex);
    }

    void JobSystem::markFailed(Job &job, const char *reason)
    {
        job.failed.store(true, std::memory_order_release);
        failures.fetch_add(1, std::memory_order_acq_rel);
        logFailure(job.debugName, reason);
    }

    void JobSystem::recordFailure(uint32_t index, uint32_t generation)
    {
        const uint64_t key = packHandle(index, generation);
        std::scoped_lock lock(failedRecordMutex);
        if (!failedRecord.insert(key).second)
            return;
        failedRecordOrder.push_back(key);
        if (failedRecordOrder.size() > kFailedRecordCapacity)
        {
            failedRecord.erase(failedRecordOrder.front());
            failedRecordOrder.pop_front();
        }
    }

    bool JobSystem::wasRecordedFailure(JobHandle handle) const
    {
        // Fast path: nothing has ever failed, so skip the lock entirely. This
        // keeps the common (clean) case off the mutex on a hot code path.
        if (failures.load(std::memory_order_acquire) == 0)
            return false;
        std::scoped_lock lock(failedRecordMutex);
        return failedRecord.count(packHandle(handle.index, handle.generation)) != 0;
    }

    void JobSystem::logFailure(const char *debugName, const char *reason)
    {
        LOG_ERROR(Logger::get(), "Job '{}' failed: {}",
                  debugName != nullptr ? debugName : "<unnamed>", reason);
    }

    JobHandle JobSystem::scheduleInline(std::function<void()> fn,
                                        std::span<const JobHandle> deps,
                                        const char *debugName)
    {
        // Serial handles are their own numbering: index is a plain counter, not
        // a pool slot, so isComplete() must short-circuit before it indexes
        // jobPool. Generation is unused (nothing is ever recycled here).
        if (serialCounter == 0xFFFFFFFF)
            serialCounter = 0;   // never mint the null sentinel
        const JobHandle handle{serialCounter++, 0};

        const bool poisoned = std::any_of(deps.begin(), deps.end(), [this](JobHandle dep) {
            return !dep.isNull() && serialFailed.count(dep.index) != 0;
        });

        if (poisoned)
        {
            // Only failed ids are recorded, so this set is bounded by the
            // failure count rather than the job count.
            serialFailed.insert(handle.index);
            failures.fetch_add(1, std::memory_order_acq_rel);
            return handle;
        }

        if (fn && !runInline(std::move(fn), debugName))
            serialFailed.insert(handle.index);

        return handle;
    }

    bool JobSystem::runInline(std::function<void()> fn, const char *debugName)
    {
        try
        {
            fn();
            return true;
        }
        catch (const std::exception &error)
        {
            failures.fetch_add(1, std::memory_order_acq_rel);
            logFailure(debugName, error.what());
        }
        catch (...)
        {
            failures.fetch_add(1, std::memory_order_acq_rel);
            logFailure(debugName, "unknown exception");
        }
        return false;
    }

    void JobSystem::finishJob(uint32_t jobIndex)
    {
        Job &job = jobPool[jobIndex];
        if (job.unfinished.fetch_sub(1, std::memory_order_acq_rel) != 1)
            return;   // parallelFor children still outstanding

        const bool poisonDependents = job.failed.load(std::memory_order_acquire);

        // Recorded BEFORE `finished` flips. A thread that observes this job as
        // already-complete takes the wasRecordedFailure path in scheduleImpl,
        // so the record has to be in place by the time completion is visible —
        // otherwise the poison would be lost exactly in the race we are closing.
        if (poisonDependents)
            recordFailure(jobIndex, job.generation.load(std::memory_order_relaxed));

        std::vector<JobHandle> toNotify;
        {
            // Same lock tryAddDependent takes: once `finished` flips under it,
            // no new dependent can slip into the list we are about to drain.
            std::scoped_lock lock(job.depMutex);
            job.finished.store(true, std::memory_order_release);
            toNotify.swap(job.dependents);
        }
        for (const JobHandle dependent : toNotify)
        {
            // Poison BEFORE the decrement: whoever observes the 1 -> 0
            // transition enqueues, and the flag must already be visible by then
            // or the dependent could start running on inputs that never arrived.
            if (poisonDependents)
            {
                jobPool[dependent.index].failed.store(true, std::memory_order_release);
                // Counted, but deliberately not logged: one failed root in a
                // wide DAG would otherwise produce a line per descendant.
                failures.fetch_add(1, std::memory_order_acq_rel);
            }
            if (jobPool[dependent.index].pendingDeps.fetch_sub(1, std::memory_order_acq_rel) == 1)
                enqueue(dependent.index);   // last dependency satisfied -> runnable
        }

        recycleSlot(jobIndex);
    }

    void JobSystem::wait(JobHandle handle)
    {
        if (isComplete(handle))
            return;

        // A thread that is neither a worker nor the main thread cannot execute
        // anything, so the loop below degenerates to a pure busy-spin that burns
        // a core — and if the awaited job is mainThreadOnly and the main thread
        // never pumps, it never terminates. Schedule from foreign threads freely
        // (enqueue routes to the global queue), but wait from a participant.
        const bool poolParticipant = (tlsWorkerIndex >= 0) || isMainThread();
        assert(poolParticipant &&
               "wait() from a non-participant thread busy-spins without executing; "
               "wait on the main thread or a pool worker");

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

        // Never split into more chunks than the pool can hold: the children are
        // allocated in one burst, so a caller-supplied grain of 1 over a large
        // count would otherwise exhaust the pool outright. Coarsening the grain
        // costs nothing — chunks beyond the worker count only add scheduling
        // overhead anyway.
        if ((count + grainSize - 1) / grainSize > kMaxParallelForChunks)
            grainSize = (count + kMaxParallelForChunks - 1) / kMaxParallelForChunks;

        const size_t numChunks = (count + grainSize - 1) / grainSize;
        if (numChunks == 1 || workers.empty())
        {
            // inline: cheaper than one job. Routed through scheduleInline in
            // serial mode so a throwing body still poisons anything that later
            // depends on the returned handle, exactly as it would threaded.
            if (workers.empty())
                return scheduleInline([&body, count] { body(0, count); }, {}, "parallelFor");

            runInline([&body, count] { body(0, count); }, "parallelFor");
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
        parent.failed.store(false, std::memory_order_relaxed);   // slots are recycled: clear stale poison
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
                // The parent MUST be decremented even when the body throws.
                // executeJob catches the exception, but this reporting call
                // lives inside the body — skipping it would leave the parent's
                // `unfinished` counter permanently above zero, so wait() on the
                // parallelFor handle would hang instead of returning. Trading a
                // terminate for a hang would be no improvement.
                try
                {
                    (*sharedBody)(begin, end);
                }
                catch (...)
                {
                    // Poison the parent before reporting in: this may be the
                    // decrement that completes it, and the flag has to be
                    // visible by then for the parent's dependents to be skipped.
                    jobPool[parentSlot].failed.store(true, std::memory_order_release);
                    finishJob(parentSlot);
                    throw;   // executeJob logs and counts it against this chunk
                }
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
