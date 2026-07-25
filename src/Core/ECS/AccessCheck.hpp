#pragma once

#include <cstdint>
#include <cstdio>
#include <cstdlib>

#ifndef NDEBUG
#define FAYE_ECS_CHECK_ACCESS 1
#else
#define FAYE_ECS_CHECK_ACCESS 0
#endif

namespace Faye::Ecs
{
#if FAYE_ECS_CHECK_ACCESS
    // Installed (as a thread_local pointer) around a scheduled system's run().
    // While set, any component touch not covered by the declared masks aborts —
    // turning an undeclared access into a deterministic crash with a name in
    // the message, even single-threaded, instead of a silent race on frame 400.
    struct ActiveAccess
    {
        uint64_t readMask = 0;
        uint64_t writeMask = 0;
        const char *systemName = nullptr;
    };

    inline thread_local const ActiveAccess *tlsActiveAccess = nullptr;

    // RAII: owns its copy so the installed pointer never dangles. Exclusive
    // (unrestricted) systems deliberately do NOT install one — a null active
    // access means "outside any policed system": unrestricted.
    struct AccessCheckScope
    {
        explicit AccessCheckScope(ActiveAccess access) : access(access)
        {
            previous = tlsActiveAccess;
            tlsActiveAccess = &this->access;
        }
        ~AccessCheckScope() { tlsActiveAccess = previous; }
        AccessCheckScope(const AccessCheckScope &) = delete;
        AccessCheckScope &operator=(const AccessCheckScope &) = delete;

        ActiveAccess access;
        const ActiveAccess *previous = nullptr;
    };

    // v1 checks TOUCH (read or write declared). Distinguishing true read-only
    // access would need const-qualified views (view<const T>) — a later
    // refinement; the scheduler is already conservative because an undeclared
    // component aborts here.
    inline void checkComponentTouch(uint64_t componentBit)
    {
        if (tlsActiveAccess == nullptr)
            return;   // outside any policed system (editor UI, flush, startup)
        if (((tlsActiveAccess->readMask | tlsActiveAccess->writeMask) & componentBit) != 0)
            return;
        std::fprintf(stderr,
                     "FATAL: system '%s' touched a component it did not declare in access()\n",
                     tlsActiveAccess->systemName ? tlsActiveAccess->systemName : "<unnamed>");
        std::abort();
    }
#endif
}
