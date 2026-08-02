#include <doctest/doctest.h>

#include <algorithm>
#include <random>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "Core/ECS/World.hpp"

using namespace Faye::Ecs;

namespace
{
    struct Position
    {
        float x = 0.0f, y = 0.0f;
    };
    struct Velocity
    {
        float dx = 0.0f, dy = 0.0f;
    };
    struct Health
    {
        int hp = 100;
    };
}

TEST_CASE("recycling bumps generation and kills stale handles")
{
    EntityRegistry registry;
    const Entity first = registry.create();
    registry.destroy(first);
    const Entity second = registry.create();     // reuses the slot
    CHECK(second.index == first.index);
    CHECK(second.generation == first.generation + 1);
    CHECK(registry.alive(second));
    CHECK_FALSE(registry.alive(first));          // the whole point
}

TEST_CASE("sparse set swap-and-pop, exact contents")
{
    SparseSet<Position> set;
    const Entity e2{2, 0}, e7{7, 0}, e4{4, 0};
    set.emplace(e2, {2.0f, 0.0f});
    set.emplace(e7, {7.0f, 0.0f});
    set.emplace(e4, {4.0f, 0.0f});

    set.remove(e7);                              // e4 must move into slot 1

    REQUIRE(set.size() == 2);
    CHECK(set.entities()[0] == 2);
    CHECK(set.entities()[1] == 4);
    CHECK(set.tryGet(e4)->x == 4.0f);
    CHECK(set.tryGet(e7) == nullptr);
    CHECK(set.tryGet(e2)->x == 2.0f);
}

TEST_CASE("fuzz: World agrees with a reference model over random ops")
{
    World world;
    std::unordered_map<uint32_t, float> reference;   // entity index -> Position.x
    std::vector<Entity> live;
    std::mt19937 rng(1234);                          // fixed seed: reproducible

    for (int op = 0; op < 5000; ++op)
    {
        const int roll = int(rng() % 100);
        if (roll < 30 || live.empty())               // create
        {
            const Entity e = world.create();
            live.push_back(e);
        }
        else
        {
            const size_t pick = rng() % live.size();
            const Entity e = live[pick];
            if (roll < 55)                            // toggle Position
            {
                if (world.has<Position>(e))
                {
                    world.remove<Position>(e);
                    reference.erase(e.index);
                }
                else
                {
                    const float x = float(rng() % 1000);
                    world.add<Position>(e, Position{x, 0.0f});
                    reference[e.index] = x;
                }
            }
            else if (roll < 75)                       // toggle Velocity (must not disturb Position)
            {
                if (world.has<Velocity>(e))
                    world.remove<Velocity>(e);
                else
                    world.add<Velocity>(e);
            }
            else                                      // destroy
            {
                world.destroy(e);
                reference.erase(e.index);
                live.erase(live.begin() + long(pick));
            }
        }
        // Full cross-check every op (cheap at this scale).
        for (const Entity e : live)
        {
            const auto it = reference.find(e.index);
            const Position *p = world.tryGet<Position>(e);
            if (it == reference.end())
            {
                CHECK(p == nullptr);
            }
            else
            {
                REQUIRE(p != nullptr);
                CHECK(p->x == it->second);
            }
        }
    }
}

TEST_CASE("destroy sweeps every pool and fires hooks")
{
    World world;
    int hookFired = 0;
    world.setRemoveHook<Health>([&hookFired](World &, Entity, void *component) {
        CHECK(static_cast<Health *>(component)->hp == 42);   // data intact at teardown
        ++hookFired;
    });

    const Entity e = world.create();
    world.add<Position>(e);
    world.add<Health>(e, Health{42});
    world.destroy(e);

    CHECK(hookFired == 1);
    CHECK_FALSE(world.alive(e));
    // Recreate on the same slot: must start clean.
    const Entity reused = world.create();
    CHECK(reused.index == e.index);
    CHECK_FALSE(world.has<Position>(reused));
    CHECK_FALSE(world.has<Health>(reused));
}

TEST_CASE("view visits exactly the intersection, driven by the smaller pool")
{
    World world;
    std::vector<Entity> both;
    for (int i = 0; i < 20; ++i)                 // 20 Position-only
        world.add<Position>(world.create());
    for (int i = 0; i < 5; ++i)                  // 5 with both
    {
        const Entity e = world.create();
        world.add<Position>(e);
        world.add<Velocity>(e, Velocity{1.0f, 0.0f});
        both.push_back(e);
    }

    int visited = 0;
    world.view<Position, Velocity>().each([&](Entity e, Position &p, Velocity &v) {
        p.x += v.dx;                             // value mutation is legal in a view
        ++visited;
        CHECK(std::find(both.begin(), both.end(), e) != both.end());
    });
    CHECK(visited == 5);
    for (const Entity e : both)
        CHECK(world.tryGet<Position>(e)->x == 1.0f);
}

TEST_CASE("pointer stability: other pools' churn never moves my component")
{
    World world;
    const Entity stable = world.create();
    Position *held = &world.add<Position>(stable, Position{9.0f, 9.0f});

    for (int i = 0; i < 1000; ++i)               // heavy churn in OTHER pools
    {
        const Entity e = world.create();
        world.add<Velocity>(e);
        world.add<Health>(e);
        if (i % 3 == 0)
            world.destroy(e);
    }
    // This is the property the sparse-set decision was made for:
    CHECK(held == world.tryGet<Position>(stable));
    CHECK(held->x == 9.0f);
}

TEST_CASE("componentId is stable per type and distinct across types")
{
    CHECK(componentId<Position>() == componentId<Position>());
    CHECK(componentId<Position>() != componentId<Velocity>());
    CHECK(componentId<Velocity>() != componentId<Health>());
}

// --- Iteration must not mutate the World (F2) -------------------------------
// View::each used to fetch spans through poolFor, which lazily resizes and
// inserts into World::pools. Concurrent read-only extractors therefore
// reallocated shared state underneath each other on first touch of a type.

namespace
{
    struct NeverPooled
    {
        int v = 0;
    };
}

TEST_CASE("iterating a never-registered component type does not create its pool")
{
    World world;
    const Entity e = world.create();
    world.add<Position>(e);

    int visits = 0;
    world.view<const Position, const NeverPooled>().each(
        [&](Entity, const Position &, const NeverPooled &) { ++visits; });

    CHECK(visits == 0);
    CHECK(world.poolIfExists<NeverPooled>() == nullptr);   // iteration stayed read-only
}

TEST_CASE("a const view still visits exactly the intersection")
{
    World world;
    const Entity both = world.create();
    world.add<Position>(both).x = 3.0f;
    world.add<Velocity>(both).dx = 4.0f;
    const Entity positionOnly = world.create();
    world.add<Position>(positionOnly);

    std::vector<Entity> seen;
    world.view<const Position, const Velocity>().each(
        [&](Entity e, const Position &p, const Velocity &v) {
            seen.push_back(e);
            CHECK(p.x == doctest::Approx(3.0f));
            CHECK(v.dx == doctest::Approx(4.0f));
        });

    REQUIRE(seen.size() == 1);
    CHECK(seen[0].index == both.index);
}

TEST_CASE("const and non-const views of the same type share one component id")
{
    // componentId<const T> is a DIFFERENT static from componentId<T>; if View
    // failed to strip const it would mint a second id for the same component
    // and every mask it appears in would disagree.
    CHECK(componentId<Position>() == componentId<std::remove_const_t<const Position>>());

    World world;
    const Entity e = world.create();
    world.add<Position>(e);

    int constVisits = 0;
    world.view<const Position>().each([&](Entity, const Position &) { ++constVisits; });
    int mutableVisits = 0;
    world.view<Position>().each([&](Entity, Position &) { ++mutableVisits; });

    CHECK(constVisits == 1);
    CHECK(mutableVisits == 1);
}

// --- componentId assignment must be atomic (F1) -----------------------------
// componentId<T>'s function-local static is thread-safe PER T, but two threads
// first-initialising DIFFERENT Ts both reach nextComponentId(). A plain
// counter++ there can hand them the same id, aliasing two component types onto
// one bit in every mask. Routed through the scheduler this stays latent
// (addSystem mints ids on the main thread), so exercise it directly.

namespace
{
    template <int N>
    struct RaceProbe
    {
        int v = 0;
    };

    template <int... Ns>
    std::vector<ComponentId> mintConcurrently(std::integer_sequence<int, Ns...>)
    {
        constexpr size_t kCount = sizeof...(Ns);
        std::atomic<int> ready{0};
        std::atomic<bool> go{false};
        std::vector<ComponentId> ids(kCount);
        std::vector<std::thread> threads;

        const auto mint = [&](size_t slot, ComponentId (*fn)()) {
            ready.fetch_add(1);
            while (!go.load(std::memory_order_acquire))
                std::this_thread::yield();   // release all threads together
            ids[slot] = fn();
        };

        size_t slot = 0;
        (threads.emplace_back(mint, slot++, +[] { return componentId<RaceProbe<Ns>>(); }), ...);

        while (size_t(ready.load()) < kCount)
            std::this_thread::yield();
        go.store(true, std::memory_order_release);
        for (std::thread &t : threads)
            t.join();
        return ids;
    }
}

TEST_CASE("concurrent first-touch of distinct component types yields distinct ids")
{
    const std::vector<ComponentId> ids =
        mintConcurrently(std::integer_sequence<int, 0, 1, 2, 3, 4, 5, 6, 7>{});

    std::vector<ComponentId> unique = ids;
    std::sort(unique.begin(), unique.end());
    unique.erase(std::unique(unique.begin(), unique.end()), unique.end());

    CHECK(unique.size() == ids.size());   // no two types share a bit
}
