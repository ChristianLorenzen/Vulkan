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

    inline void reportAccessViolation(const char *what)
    {
        std::fprintf(stderr, "FATAL: system '%s' %s\n",
                     tlsActiveAccess->systemName ? tlsActiveAccess->systemName : "<unnamed>",
                     what);
        std::abort();
    }

    // Read access: satisfied by declaring EITHER read<T> or write<T>, since a
    // writer may obviously also read.
    inline void checkComponentRead(uint64_t componentBit)
    {
        if (tlsActiveAccess == nullptr)
            return;   // outside any policed system (editor UI, flush, startup)
        if (((tlsActiveAccess->readMask | tlsActiveAccess->writeMask) & componentBit) != 0)
            return;
        reportAccessViolation("read a component it did not declare in access()");
    }

    // Write access: satisfied ONLY by write<T>. This is the check that makes
    // the scheduler's central assumption enforceable — conflicts() lets any
    // number of readers run concurrently, so a system that declares read<T> and
    // then mutates T races every one of them. Reaching here means the caller
    // obtained a mutable reference; whether it actually stored through it is
    // not knowable, so we treat acquiring the capability as the violation.
    inline void checkComponentWrite(uint64_t componentBit)
    {
        if (tlsActiveAccess == nullptr)
            return;
        if ((tlsActiveAccess->writeMask & componentBit) != 0)
            return;
        reportAccessViolation(
            "took a mutable reference to a component it only declared as read<> "
            "(use view<const T> for read-only iteration)");
    }
#endif
}
