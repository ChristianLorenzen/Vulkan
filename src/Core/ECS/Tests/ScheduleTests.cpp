// Phase 7 — system scheduling, conflict math, deferred structural changes, and
// the fixed-timestep accumulator. Everything here is headless and fast.
#include <doctest/doctest.h>

#include <mutex>
#include <string>
#include <vector>

#include "Core/ECS/SystemSchedule.hpp"
#include "Core/Jobs/JobSystem.hpp"
#include "Core/Time/FixedStepper.hpp"

using namespace Faye::Ecs;

namespace
{
    struct CompA
    {
        int v = 0;
    };
    struct CompB
    {
        int v = 0;
    };

    // Probe system: declares `access`, appends `label` to a shared log when it
    // runs (guarded, since the scheduler may run it on a worker thread).
    class ProbeSystem final : public ISystem
    {
    public:
        ProbeSystem(std::string label, SystemAccess access,
                    std::vector<std::string> &log, std::mutex &logMutex)
            : label(std::move(label)), declared(std::move(access)), log(log), logMutex(logMutex) {}

        const char *name() const override { return label.c_str(); }
        SystemAccess access() const override { return declared; }
        void run(World &, const Faye::EngineContext &, Faye::Jobs::JobSystem &,
                 CommandBuffer &) override
        {
            std::scoped_lock lock(logMutex);
            log.push_back(label);
        }

    private:
        std::string label;
        SystemAccess declared;
        std::vector<std::string> &log;
        std::mutex &logMutex;
    };
}

TEST_CASE("access conflict rules")
{
    const CompiledAccess readA = compileAccess(SystemAccess{}.read<CompA>());
    const CompiledAccess writeA = compileAccess(SystemAccess{}.write<CompA>());
    const CompiledAccess writeB = compileAccess(SystemAccess{}.write<CompB>());
    const CompiledAccess excl = compileAccess(SystemAccess{}.exclusive());

    CHECK_FALSE(conflicts(readA, readA));    // readers coexist
    CHECK(conflicts(writeA, writeA));        // W/W
    CHECK(conflicts(writeA, readA));         // W/R
    CHECK(conflicts(readA, writeA));         // R/W
    CHECK_FALSE(conflicts(writeA, writeB));  // disjoint writers
    CHECK(conflicts(excl, readA));           // exclusive vs anything
}

TEST_CASE("conflicting systems observe registration order, every run")
{
    Faye::Jobs::JobSystem jobs(4);
    World world;
    Faye::EngineContext ctx{};
    std::vector<std::string> log;
    std::mutex logMutex;

    SystemSchedule schedule;
    schedule.addSystem(Stage::Update, std::make_unique<ProbeSystem>(
                                          "first", SystemAccess{}.write<CompA>(), log, logMutex));
    schedule.addSystem(Stage::Update, std::make_unique<ProbeSystem>(
                                          "second", SystemAccess{}.write<CompA>(), log, logMutex));

    for (int run = 0; run < 1000; ++run)   // shake out races
    {
        log.clear();
        schedule.runStage(Stage::Update, world, ctx, jobs);
        REQUIRE(log == std::vector<std::string>{"first", "second"});
    }
}

TEST_CASE("non-conflicting systems both run (disjoint writers)")
{
    Faye::Jobs::JobSystem jobs(4);
    World world;
    Faye::EngineContext ctx{};
    std::vector<std::string> log;
    std::mutex logMutex;

    SystemSchedule schedule;
    schedule.addSystem(Stage::Update, std::make_unique<ProbeSystem>(
                                          "a", SystemAccess{}.write<CompA>(), log, logMutex));
    schedule.addSystem(Stage::Update, std::make_unique<ProbeSystem>(
                                          "b", SystemAccess{}.write<CompB>(), log, logMutex));

    schedule.runStage(Stage::Update, world, ctx, jobs);
    REQUIRE(log.size() == 2);   // both ran; order unconstrained (no dependency edge)
}

TEST_CASE("command buffer: placeholder create+add, tolerant double destroy")
{
    World world;
    CommandBuffer commands;

    const Entity pending = commands.createEntity();   // placeholder handle
    commands.add<CompA>(pending, CompA{7});

    const Entity doomed = world.create();
    commands.destroyEntity(doomed);
    commands.destroyEntity(doomed);   // second one: no-op, no crash

    commands.flush(world);

    CHECK_FALSE(world.alive(doomed));
    int matches = 0;
    world.view<CompA>().each([&](Entity, CompA &a)
                             {
        CHECK(a.v == 7);
        ++matches; });
    CHECK(matches == 1);   // the created entity is real now
}

TEST_CASE("fixed stepper: tick counts and clamping")
{
    Faye::FixedStepper stepper(1.0 / 60.0);
    CHECK(stepper.advance(1.0 / 60.0) == 1);   // one exact step
    CHECK(stepper.advance(2.0 / 60.0) == 2);   // slow frame: catch up twice
    CHECK(stepper.advance(0.008) == 0);        // fast frame: accumulate...
    CHECK(stepper.advance(0.009) == 1);        // ...and fire on the next
    CHECK(stepper.advance(10.0) == 15);        // clamped to 0.25s => 15 ticks, not 600
}
