#pragma once

#include <cstdint>
#include <deque>
#include <mutex>

namespace Faye::Jobs
{
    // Multiple producers (any thread whose job completion makes a main-thread
    // job runnable), single consumer (thread 0 in pumpMainThread). This queue
    // is the mechanism behind the affinity rule: anything that calls Vulkan,
    // GLFW, or ImGui is scheduled here and only ever executed by the main
    // thread.
    class MainThreadQueue
    {
    public:
        void push(uint32_t jobIndex)
        {
            std::scoped_lock lock(mutex);
            jobs.push_back(jobIndex);
        }

        // Swap-out: the lock is held only for a swap, and one pump is bounded
        // to the jobs queued when it started (prevents livelock when
        // main-thread jobs schedule more main-thread jobs).
        std::deque<uint32_t> takeAll()
        {
            std::deque<uint32_t> local;
            {
                std::scoped_lock lock(mutex);
                local.swap(jobs);
            }
            return local;
        }

    private:
        std::deque<uint32_t> jobs;
        std::mutex mutex;
    };
}
