#include "Scripting/LuaScriptSystem.hpp"

#include <filesystem>
#include <vector>

#include <glm/glm.hpp>

#include "Core/Logging/Logger.hpp"
#include "Scene/Scene.hpp"
#include "quill/LogMacros.h"

namespace Faye
{

    LuaScriptSystem::LuaScriptSystem()
    {
        lua.open_libraries(
            sol::lib::base,
            sol::lib::math,
            sol::lib::string,
            sol::lib::table,
            sol::lib::io);
    }

    void LuaScriptSystem::bindScene(Scene *scene)
    {
        boundScene = scene;
        if (scene == nullptr)
        {
            return;
        }

        auto &world = scene->getWorld();

        world.types().registerType<LuaScriptComponent, Ecs::Clone::skip>("Lua Script");

        // Teardown channel: fires on unloadScript, entity destruction, the
        // editor's type-registry remove, and scene teardown. Calling onDestroy
        // then resetting the component drops the sol references, so the Lua GC
        // can collect the sandbox.
        world.setRemoveHook<LuaScriptComponent>(
            [this](Ecs::World &, Ecs::Entity entityHandle, void *raw)
            {
                auto *script = static_cast<LuaScriptComponent *>(raw);
                if (script->onDestroy.valid() && boundScene != nullptr)
                {
                    auto r = script->onDestroy(Entity{boundScene, entityHandle});
                    if (!r.valid())
                    {
                        sol::error err = r;
                        LOG_WARNING(Logger::get(), "[LuaScriptSystem] onDestroy error in '{}': {}",
                                    script->scriptPath, err.what());
                    }
                }
                *script = LuaScriptComponent{};
            });
    }

    void LuaScriptSystem::bindEngineAPI()
    {
        // ----- Entity -----
        lua.new_usertype<Entity>("Entity", sol::no_constructor,

                                 "getId", [](const Entity &e) -> uint32_t
                                 { return e.handle().index; },

                                 "getName", [](const Entity &e) -> std::string
                                 { return std::string(e.getName()); },

                                 "isValid", &Entity::isValid,

                                 "setTranslation", [](const Entity &e, float x, float y, float z)
                                 {
            TransformComponent *t = e.tryGet<TransformComponent>();
            if (t != nullptr)
                t->translation = glm::vec3(x, y, z); },

                                 "getTranslation", [](const Entity & /*e*/) -> sol::object
                                 {
                                     // Return as a table {x, y, z} — avoids needing glm bindings
                                     return sol::lua_nil; // overridden below with correct sol::state ref
                                 });

        // Override getTranslation with proper state access
        lua["Entity"]["getTranslation"] = [this](const Entity &e) -> sol::table
        {
            sol::table t = lua.create_table();
            TransformComponent *tc = e.tryGet<TransformComponent>();
            if (tc != nullptr)
            {
                t["x"] = tc->translation.x;
                t["y"] = tc->translation.y;
                t["z"] = tc->translation.z;
            }
            return t;
        };

        // Rotation accessors — stored in radians (matches TransformComponent::mat4)
        lua["Entity"]["setRotation"] = [](const Entity &e, float x, float y, float z)
        {
            TransformComponent *t = e.tryGet<TransformComponent>();
            if (t != nullptr)
                t->rotation = glm::vec3(x, y, z);
        };

        lua["Entity"]["setRotationY"] = [](const Entity &e, float y)
        {
            TransformComponent *t = e.tryGet<TransformComponent>();
            if (t != nullptr)
                t->rotation.y = y;
        };

        lua["Entity"]["getRotation"] = [this](const Entity &e) -> sol::table
        {
            sol::table t = lua.create_table();
            TransformComponent *tc = e.tryGet<TransformComponent>();
            if (tc != nullptr)
            {
                t["x"] = tc->rotation.x;
                t["y"] = tc->rotation.y;
                t["z"] = tc->rotation.z;
            }
            return t;
        };

        // Engine context — read-only table of engine-level variables updated each frame.
        // Expandable: add fields to LuaEngineContext.hpp and register them here.
        lua.new_usertype<EngineContext>("EngineContext",
                                        sol::no_constructor,
                                        "dt", sol::readonly(&EngineContext::dt));
        lua["Engine"] = &engineContext;

        // ----- Scene -----
        lua.new_usertype<Scene>("Scene",
                                sol::no_constructor,

                                "findEntityByName", [](Scene &scene, const std::string &name) -> sol::optional<Entity>
                                {
            for (Ecs::Entity handle : scene.getEntities())
            {
                if (scene.getEntityName(handle) == name)
                    return scene.getEntity(handle);
            }
            return sol::nullopt; });

        // Override Lua's print to route through Quill
        lua["print"] = [](sol::variadic_args args)
        {
            std::string msg;
            for (auto &&v : args)
            {
                if (!msg.empty())
                    msg += '\t';
                msg += v.as<std::string>();
            }
            LOG_INFO(Logger::get(), "[Lua] {}", msg);
        };
    }

    void LuaScriptSystem::loadScript(Entity entity, const std::string &scriptPath, Scene *scene)
    {
        if (scene == nullptr || !entity.isValid())
            return;

        if (!std::filesystem::exists(scriptPath))
        {
            LOG_WARNING(Logger::get(), "[LuaScriptSystem] Script not found, skipping: {}", scriptPath);
            return;
        }

        // Unload any previous script on this entity
        if (isLoaded(entity))
            unloadScript(entity, scene);

        // Create a sandboxed environment inheriting from the global table
        sol::environment env(lua, sol::create, lua.globals());

        auto result = lua.safe_script_file(scriptPath, env, sol::script_pass_on_error);
        if (!result.valid())
        {
            sol::error err = result;
            LOG_ERROR(Logger::get(), "[LuaScriptSystem] Error loading '{}': {}", scriptPath, err.what());
            return;
        }

        LuaScriptComponent script;
        script.scriptPath = scriptPath;
        script.environment = std::move(env);

        // Grab optional callbacks
        sol::safe_function onStart = script.environment["onStart"];
        script.onUpdate = script.environment["onUpdate"];
        script.onDestroy = script.environment["onDestroy"];

        // Component first, then onStart: if onStart destroys its own entity,
        // the remove hook already owns cleanup.
        scene->getWorld().add<LuaScriptComponent>(entity.handle(), std::move(script));

        if (onStart.valid())
        {
            auto r = onStart(entity);
            if (!r.valid())
            {
                sol::error err = r;
                LOG_WARNING(Logger::get(), "[LuaScriptSystem] onStart error in '{}': {}", scriptPath, err.what());
            }
        }
    }

    void LuaScriptSystem::unloadScript(Entity entity, Scene *scene)
    {
        if (scene == nullptr)
            return;

        auto &world = scene->getWorld();
        if (world.has<LuaScriptComponent>(entity.handle()))
            world.remove<LuaScriptComponent>(entity.handle());   // hook runs onDestroy
    }

    void LuaScriptSystem::reloadScript(Entity entity, Scene *scene)
    {
        if (scene == nullptr)
            return;

        const auto *script = scene->getWorld().tryGet<LuaScriptComponent>(entity.handle());
        if (script == nullptr)
            return;

        std::string path = script->scriptPath;
        unloadScript(entity, scene);
        loadScript(entity, path, scene);
    }

    void LuaScriptSystem::unloadAll()
    {
        if (boundScene == nullptr)
            return;

        auto &world = boundScene->getWorld();
        auto *pool = world.poolIfExists<LuaScriptComponent>();
        if (pool == nullptr)
            return;

        std::vector<Ecs::Entity> attached;
        attached.reserve(pool->set.size());
        for (const uint32_t entityIndex : pool->set.entities())
            attached.push_back(world.entityAt(entityIndex));

        for (const Ecs::Entity entityHandle : attached)
            world.remove<LuaScriptComponent>(entityHandle);
    }

    void LuaScriptSystem::update(const EngineContext &ctx, Scene *scene)
    {
        if (!scene)
            return;

        // Sync from the shared engine context so Lua's Engine.dt
        // always matches what C++ scripts see via onUpdate ctx.
        engineContext.dt = ctx.dt;

        auto &world = scene->getWorld();
        auto *pool = world.poolIfExists<LuaScriptComponent>();
        if (pool == nullptr)
            return;

        // Snapshot first: Lua onUpdate may make structural changes.
        std::vector<Ecs::Entity> attached;
        attached.reserve(pool->set.size());
        for (const uint32_t entityIndex : pool->set.entities())
            attached.push_back(world.entityAt(entityIndex));

        for (const Ecs::Entity entityHandle : attached)
        {
            if (!world.alive(entityHandle))
                continue;
            auto *script = world.tryGet<LuaScriptComponent>(entityHandle);
            if (script == nullptr || !script->onUpdate.valid())
                continue;

            auto r = script->onUpdate(Entity{scene, entityHandle}, ctx.dt);
            if (!r.valid())
            {
                sol::error err = r;
                LOG_WARNING(Logger::get(), "[LuaScriptSystem] onUpdate error in '{}': {}", script->scriptPath, err.what());
            }
        }
    }

    bool LuaScriptSystem::isLoaded(Entity entity) const
    {
        return boundScene != nullptr &&
               boundScene->getWorld().has<LuaScriptComponent>(entity.handle());
    }

} // namespace Faye
