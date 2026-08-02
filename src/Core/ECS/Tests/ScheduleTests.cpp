// Phase 7 — system scheduling, conflict math, deferred structural changes, and
// the fixed-timestep accumulator. Everything here is headless and fast.
#include <doctest/doctest.h>

#include <atomic>
#include <memory>
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

// --- Concurrent read-only systems on a cold World (F1, F2) ------------------
// This is the shape that raced: several reader systems, scheduled with no
// edges between them (readers never conflict), all touching component types
// whose pools and ids do not exist yet. Under TSan this reproduces both the
// World::pools reallocation and the componentId counter increment.

namespace
{
    struct ColdA { int v = 0; };
    struct ColdB { int v = 0; };
    struct ColdC { int v = 0; };

    // Reader over one cold type. Declares read<T> and iterates with
    // view<const T>, so checkComponentWrite must not fire.
    template <class T>
    class ColdReaderSystem final : public ISystem
    {
    public:
        explicit ColdReaderSystem(std::atomic<int> &visits) : visits(visits) {}

        const char *name() const override { return "ColdReader"; }
        SystemAccess access() const override { return SystemAccess{}.read<T>(); }
        void run(World &world, const Faye::EngineContext &, Faye::Jobs::JobSystem &,
                 CommandBuffer &) override
        {
            world.view<const T>().each(
                [&](Entity, const T &) { visits.fetch_add(1, std::memory_order_relaxed); });
        }

    private:
        std::atomic<int> &visits;
    };
}

TEST_CASE("concurrent readers over cold component types do not race")
{
    Faye::Jobs::JobSystem jobs(4);

    for (int run = 0; run < 200; ++run)
    {
        World world;                 // fresh: no pools exist yet
        std::atomic<int> visits{0};

        SystemSchedule schedule;
        schedule.addSystem(Stage::Extract, std::make_unique<ColdReaderSystem<ColdA>>(visits));
        schedule.addSystem(Stage::Extract, std::make_unique<ColdReaderSystem<ColdB>>(visits));
        schedule.addSystem(Stage::Extract, std::make_unique<ColdReaderSystem<ColdC>>(visits));

        Faye::EngineContext ctx;
        schedule.runStage(Stage::Extract, world, ctx, jobs);

        CHECK(visits.load() == 0);   // no entities; the point is that it is race-free
        // Iteration must not have created pools for types nothing ever added.
        CHECK(world.poolIfExists<ColdA>() == nullptr);
        CHECK(world.poolIfExists<ColdB>() == nullptr);
        CHECK(world.poolIfExists<ColdC>() == nullptr);
    }
}

TEST_CASE("concurrent readers see the data they declare")
{
    Faye::Jobs::JobSystem jobs(4);

    World world;
    for (int i = 0; i < 50; ++i)
    {
        const Entity e = world.create();
        world.add<ColdA>(e);
        world.add<ColdB>(e);
    }

    std::atomic<int> visits{0};
    SystemSchedule schedule;
    schedule.addSystem(Stage::Extract, std::make_unique<ColdReaderSystem<ColdA>>(visits));
    schedule.addSystem(Stage::Extract, std::make_unique<ColdReaderSystem<ColdB>>(visits));

    Faye::EngineContext ctx;
    schedule.runStage(Stage::Extract, world, ctx, jobs);

    CHECK(visits.load() == 100);   // both readers walked all 50 entities
}
