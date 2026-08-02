#include "Core/ECS/SystemSchedule.hpp"

#include <functional>

#include "Core/ECS/AccessCheck.hpp"

namespace Faye::Ecs
{
    namespace
    {
        // Counts the nesting of "systems are in flight" for the component-id
        // tripwire in detail::nextComponentId. Disarm() is explicit rather than
        // scope-based because the window ends at the barrier, not at the end of
        // runStage — the command-buffer flush that follows is single-threaded
        // and may legitimately mint an id.
        struct ScheduleDepthGuard
        {
            ScheduleDepthGuard() { detail::scheduleDepth.fetch_add(1, std::memory_order_relaxed); }
            ~ScheduleDepthGuard() { disarm(); }

            ScheduleDepthGuard(const ScheduleDepthGuard &) = delete;
            ScheduleDepthGuard &operator=(const ScheduleDepthGuard &) = delete;

            // Idempotent, so the destructor is safe after an explicit disarm and
            // still fires if a system throws out of the scheduling loop.
            void disarm()
            {
                if (!armed)
                    return;
                armed = false;
                detail::scheduleDepth.fetch_sub(1, std::memory_order_relaxed);
            }

            bool armed = true;
        };
    }

    void SystemSchedule::addSystem(Stage stage, std::unique_ptr<ISystem> system)
    {
        auto &list = stages[size_t(stage)];

        ScheduledSystem entry;
        entry.access = compileAccess(system->access());
        entry.system = std::move(system);
        for (uint32_t earlier = 0; earlier < uint32_t(list.size()); ++earlier)
            if (conflicts(entry.access, list[earlier].access))
                entry.dependencies.push_back(earlier);
        // Transitively redundant edges (A->B, B->C, and also A->C) are harmless:
        // the job system just counts one extra already-satisfied dependency.
        list.push_back(std::move(entry));
    }

    void SystemSchedule::runStage(Stage stage, World &world, const EngineContext &ctx,
                                  Jobs::JobSystem &jobs)
    {
        auto &list = stages[size_t(stage)];
        if (list.empty())
            return;

        // Grown only between stages — the buffers must not move while jobs hold
        // pointers to them.
        if (commandBuffers.size() < list.size())
            commandBuffers.resize(list.size());

        std::vector<Jobs::JobHandle> handles(list.size());

        // Arms the debug tripwire in detail::nextComponentId for the duration of
        // the parallel section: a component type first touched from inside
        // parallel work is the shape that precedes a pool being created
        // underneath a concurrent reader. Register types at startup instead.
        // Explicitly disarmed after the barrier, because flush() replays
        // structural changes single-threaded and may legitimately create a pool.
        ScheduleDepthGuard scheduleDepthGuard;

        for (size_t i = 0; i < list.size(); ++i)
        {
            ScheduledSystem &scheduled = list[i];

            std::vector<Jobs::JobHandle> deps;
            deps.reserve(scheduled.dependencies.size());
            for (const uint32_t earlier : scheduled.dependencies)
                deps.push_back(handles[earlier]);   // already-complete handles are fine

            ISystem *system = scheduled.system.get();
            CommandBuffer *commands = &commandBuffers[i];

            // Exclusive systems declare no masks (they touch everything and run
            // alone) — installing an access guard for them would abort on the
            // first component touch, so leave them unrestricted.
            std::function<void()> body;
#if FAYE_ECS_CHECK_ACCESS
            if (!scheduled.access.isExclusive)
            {
                const ActiveAccess active{scheduled.access.readMask,
                                          scheduled.access.writeMask,
                                          system->name()};
                body = [system, &world, &ctx, &jobs, commands, active]
                {
                    AccessCheckScope guard(active);
                    system->run(world, ctx, jobs, *commands);
                };
            }
            else
            {
                body = [system, &world, &ctx, &jobs, commands]
                { system->run(world, ctx, jobs, *commands); };
            }
#else
            body = [system, &world, &ctx, &jobs, commands]
            { system->run(world, ctx, jobs, *commands); };
#endif

            handles[i] = scheduled.access.needsMainThread
                             ? jobs.scheduleMainThread(std::move(body), deps, system->name())
                             : jobs.schedule(std::move(body), deps, system->name());
        }

        // The stage BARRIER. On the main thread, wait() pumps the main-thread
        // queue and executes worker jobs itself — the main thread is a full
        // participant, not an idle waiter.
        jobs.waitAll(handles);
        scheduleDepthGuard.disarm();

        // Structural changes: single-threaded replay, registration order —
        // deterministic by construction.
        for (size_t i = 0; i < list.size(); ++i)
            commandBuffers[i].flush(world);
    }
}
