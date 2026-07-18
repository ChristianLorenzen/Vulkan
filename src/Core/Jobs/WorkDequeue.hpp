#pragma once

#include <cstdint>
#include <deque>
#include <mutex>

namespace Faye::Jobs
{
    // v1: a mutex-guarded deque with the work-stealing access pattern baked
    // into the interface. Owner pushes/pops the bottom; thieves steal the top.
    // Interface-compatible with a Chase-Lev lock-free deque for a later,
    // profile-gated upgrade.
    class WorkDequeue
    {
    public:
        void pushBottom(uint32_t jobIndex)
        {
            std::scoped_lock lock(mutex);
            jobs.push_back(jobIndex);
        }

        bool popBottom(uint32_t &outJobIndex)
        {
            std::scoped_lock lock(mutex);
            if (jobs.empty())
                return false;
            outJobIndex = jobs.back();
            jobs.pop_back();
            return true;
        }

        bool stealTop(uint32_t &outJobIndex)
        {
            std::scoped_lock lock(mutex);
            if (jobs.empty())
                return false;
            outJobIndex = jobs.front();
            jobs.pop_front();
            return true;
        }

    private:
        std::deque<uint32_t> jobs;
        mutable std::mutex mutex;
    };
}