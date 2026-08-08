// Phase 5 risk-register tests (docs/ecs/07-integration-and-migration.md
// §Phase 5): "write tests for destroy-while-script-attached, reload-then-
// destroy". Uses attachBuiltinScript exclusively — no dlopen/.so mechanics —
// because the teardown ordering under test lives entirely in
// ScriptSystem::teardown(), which is identical for .so-backed and built-in
// scripts; only the library-unload step differs.
#include <doctest/doctest.h>

#include "engine/Scene/Scene.hpp"
#include "engine/Scripting/ScriptComponents.hpp"
#include "engine/Scripting/ScriptSystem.hpp"
using namespace Faye;

namespace
{
    struct LifecycleCounts
    {
        int starts = 0;
        int updates = 0;
        int destroys = 0;
    };

    class CountingScript final : public IScript
    {
    public:
        explicit CountingScript(LifecycleCounts &counts) : counts(counts) {}

        void onStart(Entity, Scene *) override { ++counts.starts; }
        void onUpdate(Entity, Scene *, const EngineContext &) override { ++counts.updates; }
        void onDestroy(Entity, Scene *) override { ++counts.destroys; }

    private:
        LifecycleCounts &counts;
    };
}

TEST_CASE("destroy-while-script-attached: entity destruction tears the script down exactly once")
{
    Scene scene;
    ScriptSystem scripts;
    scripts.bindScene(&scene);

    LifecycleCounts counts;
    Entity entity = scene.createEntity();
    scripts.attachBuiltinScript(entity, new CountingScript(counts), "Counting");
    REQUIRE(counts.starts == 1);

    // Destroying the entity directly (not via unloadScript) is the leak
    // scenario Phase 5 fixes: the World's remove hook must still fire.
    entity.destroy();

    CHECK(counts.destroys == 1);
    CHECK_FALSE(entity.isValid());
}

TEST_CASE("reload-then-destroy: unload/reattach tears down the old instance once, "
          "destroy tears down only the new one")
{
    Scene scene;
    ScriptSystem scripts;
    scripts.bindScene(&scene);

    LifecycleCounts oldCounts;
    LifecycleCounts newCounts;
    Entity entity = scene.createEntity();

    scripts.attachBuiltinScript(entity, new CountingScript(oldCounts), "Old");
    REQUIRE(oldCounts.starts == 1);

    // The unload half of ScriptSystem::reloadScript.
    scripts.unloadScript(entity);
    CHECK(oldCounts.destroys == 1);

    // The load half — a fresh instance, as reloadScript would attach.
    scripts.attachBuiltinScript(entity, new CountingScript(newCounts), "New");
    CHECK(newCounts.starts == 1);
    CHECK(oldCounts.destroys == 1);   // still exactly one: no double-teardown

    entity.destroy();

    CHECK(newCounts.destroys == 1);
    CHECK(oldCounts.destroys == 1);   // the dead instance is never touched again
}

TEST_CASE("attachBuiltinScript on an already-scripted entity tears down the previous script first")
{
    Scene scene;
    ScriptSystem scripts;
    scripts.bindScene(&scene);

    LifecycleCounts first;
    LifecycleCounts second;
    Entity entity = scene.createEntity();

    scripts.attachBuiltinScript(entity, new CountingScript(first), "First");
    scripts.attachBuiltinScript(entity, new CountingScript(second), "Second");   // no explicit unload

    CHECK(first.destroys == 1);
    CHECK(second.starts == 1);
    CHECK(second.destroys == 0);

    entity.destroy();
    CHECK(second.destroys == 1);
    CHECK(first.destroys == 1);   // unaffected by the later destroy
}

TEST_CASE("update drives onUpdate only for live, attached entities")
{
    Scene scene;
    ScriptSystem scripts;
    scripts.bindScene(&scene);

    LifecycleCounts counts;
    Entity entity = scene.createEntity();
    scripts.attachBuiltinScript(entity, new CountingScript(counts), "Counting");

    EngineContext ctx;
    ctx.dt = 1.0f / 60.0f;
    scripts.update(ctx, &scene);
    scripts.update(ctx, &scene);
    CHECK(counts.updates == 2);

    entity.destroy();
    scripts.update(ctx, &scene);   // dead entity: must not crash or double-count
    CHECK(counts.updates == 2);
}

TEST_CASE("unloadAll tears down every attached script exactly once")
{
    Scene scene;
    ScriptSystem scripts;
    scripts.bindScene(&scene);

    LifecycleCounts a, b, c;
    Entity ea = scene.createEntity();
    Entity eb = scene.createEntity();
    Entity ec = scene.createEntity();
    scripts.attachBuiltinScript(ea, new CountingScript(a), "A");
    scripts.attachBuiltinScript(eb, new CountingScript(b), "B");
    scripts.attachBuiltinScript(ec, new CountingScript(c), "C");

    // The ScriptingSystem::OnStop pattern: unload everything while the scene
    // and this system are still alive, so destruction order afterward can't
    // dangle a captured `this` inside a remove hook.
    scripts.unloadAll();

    CHECK(a.destroys == 1);
    CHECK(b.destroys == 1);
    CHECK(c.destroys == 1);
    CHECK_FALSE(scene.getWorld().has<NativeScriptComponent>(ea.handle()));
    CHECK_FALSE(scene.getWorld().has<NativeScriptComponent>(eb.handle()));
    CHECK_FALSE(scene.getWorld().has<NativeScriptComponent>(ec.handle()));

    // Idempotent: nothing left to tear down a second time.
    scripts.unloadAll();
    CHECK(a.destroys == 1);
}

TEST_CASE("scene destruction with scripts attached: unloadAll before scope end avoids dangling hooks")
{
    LifecycleCounts counts;
    {
        Scene scene;
        ScriptSystem scripts;
        scripts.bindScene(&scene);

        Entity entity = scene.createEntity();
        scripts.attachBuiltinScript(entity, new CountingScript(counts), "Counting");

        // Mirrors ScriptingSystem::OnStop: tear down while both scene and
        // scripts are alive, then let both go out of scope in either order.
        scripts.unloadAll();
    }

    CHECK(counts.destroys == 1);
}
