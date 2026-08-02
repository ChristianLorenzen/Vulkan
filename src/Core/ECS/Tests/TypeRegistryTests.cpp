#include <doctest/doctest.h>

#include <string>

#include "Core/ECS/World.hpp"

using namespace Faye::Ecs;

namespace
{
    struct Foo
    {
        int v = 1;
    };
    struct Bar
    {
        int v = 2;
    };
}

TEST_CASE("registry round-trips through the erased pointers")
{
    World world;
    world.types().registerType<Foo>("Foo");
    world.types().registerType<Bar>("Bar");

    const Entity e = world.create();
    const ComponentTypeInfo &fooInfo = world.types().info(componentId<Foo>());

    CHECK_FALSE(fooInfo.has(world, e));
    fooInfo.addDefault(world, e);
    CHECK(fooInfo.has(world, e));
    CHECK(world.has<Foo>(e));                        // erased add == typed add

    void *raw = fooInfo.tryGetRaw(world, e);
    REQUIRE(raw != nullptr);
    CHECK(static_cast<Foo *>(raw)->v == 1);
    CHECK(raw == world.tryGet<Foo>(e));              // same object, both routes

    fooInfo.remove(world, e);
    CHECK_FALSE(world.has<Foo>(e));
    CHECK(fooInfo.tryGetRaw(world, e) == nullptr);
}

TEST_CASE("all() exposes registered types by id with correct names")
{
    World world;
    world.types().registerType<Foo>("Foo");
    world.types().registerType<Bar>("Bar");

    CHECK(std::string(world.types().info(componentId<Foo>()).name) == "Foo");
    CHECK(std::string(world.types().info(componentId<Bar>()).name) == "Bar");
    CHECK(world.types().all().size() >= 2);
}
