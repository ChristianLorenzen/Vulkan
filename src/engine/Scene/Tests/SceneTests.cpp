// Integration tests for the ECS migration: Scene is a thin facade over
// Ecs::World. Callers hold generational Ecs::Entity handles directly (Phase 5
// dissolved the legacy monotonic-id bridge from Phase 3) — a stale handle
// safely reads as dead instead of aliasing a recycled slot.
#include <doctest/doctest.h>

#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "engine/Scene/Scene.hpp"
using Faye::Ecs::Entity;
using Faye::Scene;

using Faye::CameraComponent;
using Faye::MeshRendererComponent;
using Faye::PointLightComponent;
using Faye::TransformComponent;
using Faye::WaterComponent;

TEST_CASE("createEntity: default names, creation order")
{
    Scene scene;
    const Faye::Entity first = scene.createEntity();
    const Faye::Entity second = scene.createEntity("Player");

    CHECK(scene.getEntityName(first.handle()) == "Entity 1");
    CHECK(second.getName() == "Player");
    CHECK(scene.getEntities() == std::vector<Entity>{first.handle(), second.handle()});
}

TEST_CASE("destroy: stale handles stay dead, slot reuse bumps generation")
{
    Scene scene;
    const Entity doomed = scene.createEntity().handle();
    scene.add<TransformComponent>(doomed);
    scene.destroyEntity(doomed);

    CHECK_FALSE(scene.isValid(doomed));
    CHECK(scene.tryGet<TransformComponent>(doomed) == nullptr);
    CHECK(scene.getEntityName(doomed).empty());
    CHECK_THROWS_AS(scene.add<TransformComponent>(doomed), std::runtime_error);

    // The recycled slot's handle differs (higher generation), so it never
    // aliases the dead one even if the index is reused.
    const Entity next = scene.createEntity().handle();
    CHECK(next != doomed);

    // Destroying an already-dead or null handle is a silent no-op.
    scene.destroyEntity(doomed);
    scene.destroyEntity(Entity::null());
}

TEST_CASE("add is add-or-get: re-adding returns the existing component untouched")
{
    Scene scene;
    const Entity entity = scene.createEntity().handle();

    auto &transform = scene.add<TransformComponent>(entity);
    transform.translation.x = 5.0f;
    auto &again = scene.add<TransformComponent>(entity);

    CHECK(&again == &transform);
    CHECK(again.translation.x == 5.0f);

    // addMesh re-assigns the handle on the existing component (old behavior).
    scene.addMesh(entity, Faye::ModelHandle{7});
    auto &mesh = scene.addMesh(entity, Faye::ModelHandle{9});
    CHECK(mesh.modelHandle.value == 9);
}

TEST_CASE("remove: silent no-op when absent, clears only the removed component")
{
    Scene scene;
    const Entity entity = scene.createEntity().handle();
    scene.add<TransformComponent>(entity);
    scene.add<WaterComponent>(entity);

    scene.remove<MeshRendererComponent>(entity);                    // not present: no-op
    scene.remove<WaterComponent>(entity);

    CHECK(scene.tryGet<TransformComponent>(entity) != nullptr);
    CHECK(scene.tryGet<WaterComponent>(entity) == nullptr);
    scene.remove<WaterComponent>(entity);                   // already gone: no-op
    scene.remove<TransformComponent>(Entity::null());       // null handle: no-op
}

TEST_CASE("scene worlds ship with the engine and scripting component types registered")
{
    Scene scene;
    const Entity entity = scene.createEntity().handle();
    scene.add<WaterComponent>(entity);
    scene.add<TransformComponent>(entity);

    // The inspector's data path: enumerate the type registry, probe with the
    // erased ops. Every engine type must be present by name.
    Faye::Ecs::World &world = scene.getWorld();

    std::vector<std::string_view> attached;
    std::vector<std::string_view> registered;
    for (const Faye::Ecs::ComponentTypeInfo &info : world.types().all())
    {
        if (info.name == nullptr)
            continue;
        registered.push_back(info.name);
        if (info.has(world, entity))
            attached.push_back(info.name);
    }

    // Scripting types are only registered once ScriptSystem/LuaScriptSystem
    // bind this scene (ScriptingSystem::OnPostInit in the real engine); a
    // bare Scene only carries the engine component types.
    CHECK(registered == std::vector<std::string_view>{
                            "Transform", "Mesh", "Camera", "Water",
                            "Point Light", "Directional Light",
                            "Post Process Stack", "RigidBody2D"});
    CHECK(attached == std::vector<std::string_view>{"Transform", "Water"});

    // EntityMetadata stays out of the registry (drawn as the name field, not
    // a component panel) even though every entity carries it.
    CHECK(world.tryGet<Scene::EntityMetadata>(entity) != nullptr);
}

TEST_CASE("erased remove via the type registry keeps the primary-camera invariant")
{
    Scene scene;
    const Entity entity = scene.createEntity().handle();
    scene.addCamera(entity);
    REQUIRE(scene.getPrimaryCameraHandle() == entity);

    // The editor's remove button goes through the type registry, not
    // Scene::remove<CameraComponent> — the World's remove hook must reset the primary.
    Faye::Ecs::World &world = scene.getWorld();
    const Faye::Ecs::ComponentTypeInfo &cameraInfo =
        world.types().info(Faye::Ecs::componentId<Faye::CameraComponent>());
    cameraInfo.remove(world, entity);

    CHECK(scene.getPrimaryCameraHandle() == Entity::null());
    CHECK(scene.getPrimaryCamera() == nullptr);
}

TEST_CASE("primary camera: first camera wins, explicit primary switches, death resets")
{
    Scene scene;
    const Entity a = scene.createEntity().handle();
    const Entity b = scene.createEntity().handle();

    scene.addCamera(a);                          // first camera: primary by default
    CHECK(scene.getPrimaryCameraHandle() == a);

    scene.addCamera(b, true);                    // explicit primary switches
    CHECK(scene.getPrimaryCameraHandle() == b);
    CHECK_FALSE(scene.tryGet<CameraComponent>(a)->primary);
    CHECK(scene.tryGet<CameraComponent>(b)->primary);

    scene.destroyEntity(b);                      // primary died: nothing primary
    CHECK(scene.getPrimaryCameraHandle() == Entity::null());
    CHECK(scene.getPrimaryCamera() == nullptr);

    scene.setPrimaryCamera(a);
    CHECK(scene.getPrimaryCameraHandle() == a);
    scene.remove<CameraComponent>(a);                       // component removed: reset again
    CHECK(scene.getPrimaryCameraHandle() == Entity::null());
}

// Golden oracle for the Phase 6 extraction rewrite: render extraction now
// iterates world.view<...>() directly (getRenderableViews/getPointLightViews
// are gone). This pins the set — and, for meshes, the order — the extractors
// see, so a regression in view iteration surfaces here rather than on screen.
TEST_CASE("world views yield the renderable/point-light entities in extraction order")
{
    Scene scene;
    std::vector<Entity> expectedMeshes;
    for (int i = 0; i < 4; ++i)
    {
        const Entity entity = scene.createEntity().handle();
        scene.add<TransformComponent>(entity);
        if (i % 2 == 0)
        {
            scene.addMesh(entity);
            expectedMeshes.push_back(entity);
        }
        else
        {
            scene.add<PointLightComponent>(entity);
        }
    }

    // The mesh extractor walks exactly this view. The driver is the smaller
    // (mesh) pool, whose dense order is component-insertion = creation order
    // here, so submission order matches the pre-ECS getRenderableViews().
    std::vector<Entity> renderables;
    scene.getWorld().view<Faye::TransformComponent, Faye::MeshRendererComponent>().each(
        [&](Entity entity, Faye::TransformComponent &, Faye::MeshRendererComponent &)
        { renderables.push_back(entity); });

    REQUIRE(renderables.size() == 2);
    CHECK(renderables[0] == expectedMeshes[0]);
    CHECK(renderables[1] == expectedMeshes[1]);

    std::vector<Entity> lights;
    scene.getWorld().view<Faye::TransformComponent, Faye::PointLightComponent>().each(
        [&](Entity entity, Faye::TransformComponent &, Faye::PointLightComponent &)
        { lights.push_back(entity); });
    CHECK(lights.size() == 2);
}

TEST_CASE("entity facade forwards through the scene")
{
    Scene scene;
    Faye::Entity entity = scene.createEntity("Facade");

    entity.add<TransformComponent>().translation.y = 3.0f;
    CHECK(entity.tryGet<TransformComponent>()->translation.y == 3.0f);

    entity.setName("Renamed");
    CHECK(scene.getEntityName(entity.handle()) == "Renamed");
    entity.setName("");                          // empty name falls back to default
    CHECK(entity.getName() == "Entity " + std::to_string(entity.handle().index + 1));

    entity.destroy();
    CHECK_FALSE(entity.isValid());
}

TEST_CASE("destroying the scene destroys every remaining entity through the normal path")
{
    Faye::Ecs::Entity survivorHandle;
    bool hookFired = false;

    {
        Scene scene;
        const Faye::Entity survivor = scene.createEntity();
        survivorHandle = survivor.handle();

        scene.getWorld().setRemoveHook<Faye::TransformComponent>(
            [&hookFired](Faye::Ecs::World &, Faye::Ecs::Entity, void *) { hookFired = true; });
        survivor.add<TransformComponent>();
        // Scene's destructor must still run world.destroy() on every
        // remaining entity (not just drop the World), so remove hooks —
        // which script/camera teardown depends on — fire even at shutdown.
    }

    CHECK(hookFired);
}

// ---------------------------------------------------------------------------
// Scene identity. Two .faye files must never claim the same uuid: it is the
// only stable handle a future scene reference / asset record can key on.
// ---------------------------------------------------------------------------
TEST_CASE("Save As forks the scene uuid only when it creates a copy")
{
    // A scene already on disk, written elsewhere -> that is a duplicate.
    CHECK(Faye::sceneSaveNeedsNewUuid("assets/scenes/a.faye", "assets/scenes/b.faye"));

    // Written back over itself -> still the same scene.
    CHECK_FALSE(Faye::sceneSaveNeedsNewUuid("assets/scenes/a.faye", "assets/scenes/a.faye"));

    // Never persisted -> no file claims its id yet, so nothing to collide with.
    CHECK_FALSE(Faye::sceneSaveNeedsNewUuid("", "assets/scenes/b.faye"));

    // Degenerate: no target path is not a save at all.
    CHECK_FALSE(Faye::sceneSaveNeedsNewUuid("assets/scenes/a.faye", ""));
}

TEST_CASE("regenerateSceneUuid mints a distinct identity")
{
    Faye::Scene scene("Scene");
    const Faye::Uuid original = scene.getSceneUuid();
    CHECK_FALSE(original.isNull());

    scene.regenerateSceneUuid();
    CHECK(scene.getSceneUuid() != original);

    // Clearing entities must NOT touch identity -- the load path clears before
    // the reader supplies the uuid from the file, and File > New mints its own.
    const Faye::Uuid afterRegenerate = scene.getSceneUuid();
    scene.createEntity("doomed");
    scene.clear();
    CHECK(scene.getSceneUuid() == afterRegenerate);
}

TEST_CASE("freshly constructed scenes do not share an identity")
{
    Faye::Scene first("A");
    Faye::Scene second("B");
    CHECK(first.getSceneUuid() != second.getSceneUuid());
}
