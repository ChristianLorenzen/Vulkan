#pragma once

#include "Core/ECS/CommandBuffer.hpp"
#include "Core/ECS/SystemAccess.hpp"
#include "Core/EngineContext.hpp"
#include "Core/Jobs/JobSystem.hpp"

namespace Faye::Ecs
{
    class World;

    // Each system declares what it reads/writes; the schedule
    // trusts that promise to decide concurrency.
    class ISystem
    {
    public:
        virtual ~ISystem() = default;

        virtual const char *name() const = 0;      // profiling + abort messages
        virtual SystemAccess access() const = 0;   // the promise the scheduler trusts

        // `jobs` lets a system parallelFor its OWN iteration (independent of
        // whether the scheduler overlaps it with other systems). `commands` is
        // the only legal channel for structural change during a stage.
        virtual void run(World &world, const EngineContext &ctx,
                         Jobs::JobSystem &jobs, CommandBuffer &commands) = 0;
    };
}
