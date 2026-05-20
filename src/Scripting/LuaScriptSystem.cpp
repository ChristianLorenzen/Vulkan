#include "Scripting/LuaScriptSystem.hpp"

#include <filesystem>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
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

void LuaScriptSystem::bindEngineAPI()
{
    // ----- Entity -----
    lua.new_usertype<Entity>("Entity",
        sol::no_constructor,

        "getId", [](const Entity &e) -> uint32_t
        {
            return e.id();
        },

        "getName", [](const Entity &e) -> std::string
        {
            return std::string(e.getName());
        },

        "isValid", &Entity::isValid,

        "setTranslation", [](const Entity &e, float x, float y, float z)
        {
            TransformComponent *t = e.tryGetTransform();
            if (t != nullptr)
                t->translation = glm::vec3(x, y, z);
        },

        "getTranslation", [](const Entity & /*e*/) -> sol::object
        {
            // Return as a table {x, y, z} — avoids needing glm bindings
            return sol::lua_nil; // overridden below with correct sol::state ref
        }
    );

    // Override getTranslation with proper state access
    lua["Entity"]["getTranslation"] = [this](const Entity &e) -> sol::table
    {
        sol::table t = lua.create_table();
        TransformComponent *tc = e.tryGetTransform();
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
        TransformComponent *t = e.tryGetTransform();
        if (t != nullptr)
            t->rotation = glm::vec3(x, y, z);
    };

    lua["Entity"]["setRotationY"] = [](const Entity &e, float y)
    {
        TransformComponent *t = e.tryGetTransform();
        if (t != nullptr)
            t->rotation.y = y;
    };

    lua["Entity"]["getRotation"] = [this](const Entity &e) -> sol::table
    {
        sol::table t = lua.create_table();
        TransformComponent *tc = e.tryGetTransform();
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
        "dt",   sol::readonly(&EngineContext::dt),
        "time", sol::readonly(&EngineContext::time)
    );
    lua["Engine"] = &engineContext;

    // ----- Scene -----
    lua.new_usertype<Scene>("Scene",
        sol::no_constructor,

        "findEntityByName", [](Scene &scene, const std::string &name) -> sol::optional<Entity>
        {
            for (EntityId id : scene.getEntities())
            {
                if (scene.getEntityName(id) == name)
                    return scene.getEntity(id);
            }
            return sol::nullopt;
        }
    );

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
        LOG_INFO(Logger::getInstance(), "[Lua] {}", msg);
    };
}

void LuaScriptSystem::loadScript(Entity entity, const std::string &scriptPath, Scene *scene)
{
    if (!entity.isValid())
        return;

    if (!std::filesystem::exists(scriptPath))
    {
        LOG_WARNING(Logger::getInstance(), "[LuaScriptSystem] Script not found, skipping: {}", scriptPath);
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
        LOG_ERROR(Logger::getInstance(), "[LuaScriptSystem] Error loading '{}': {}", scriptPath, err.what());
        return;
    }

    LuaScriptEntry entry;
    entry.scriptPath = scriptPath;
    entry.scriptEnv  = std::move(env);

    // Grab optional callbacks
    sol::safe_function onStart   = entry.scriptEnv["onStart"];
    entry.onUpdate               = entry.scriptEnv["onUpdate"];
    entry.onDestroy              = entry.scriptEnv["onDestroy"];

    // Call onStart if present
    if (onStart.valid())
    {
        auto r = onStart(entity);
        if (!r.valid())
        {
            sol::error err = r;
            LOG_WARNING(Logger::getInstance(), "[LuaScriptSystem] onStart error in '{}': {}", scriptPath, err.what());
        }
    }

    scripts.emplace(entity.id(), std::move(entry));
    (void)scene; // scene is available for future API extensions
}

void LuaScriptSystem::unloadScript(Entity entity, Scene *scene)
{
    auto it = scripts.find(entity.id());
    if (it == scripts.end())
        return;

    LuaScriptEntry &entry = it->second;
    if (entry.onDestroy.valid())
    {
        auto r = entry.onDestroy(entity);
        if (!r.valid())
        {
            sol::error err = r;
            LOG_WARNING(Logger::getInstance(), "[LuaScriptSystem] onDestroy error in '{}': {}", entry.scriptPath, err.what());
        }
    }

    scripts.erase(it);
    (void)scene;
}

void LuaScriptSystem::reloadScript(Entity entity, Scene *scene)
{
    auto it = scripts.find(entity.id());
    if (it == scripts.end())
        return;

    std::string path = it->second.scriptPath;
    unloadScript(entity, scene);
    loadScript(entity, path, scene);
}

void LuaScriptSystem::update(const EngineContext &ctx, Scene *scene)
{
    if (!scene)
        return;

    // Sync from the shared engine context so Lua's Engine.dt/Engine.time
    // always matches what C++ scripts see via onUpdate ctx.
    engineContext.dt   = ctx.dt;
    engineContext.time = ctx.time;

    for (auto &[entityId, entry] : scripts)
    {
        if (!entry.onUpdate.valid())
            continue;

        Entity entity = scene->getEntity(entityId);
        if (!entity.isValid())
            continue;

        auto r = entry.onUpdate(entity, ctx.dt);
        if (!r.valid())
        {
            sol::error err = r;
            LOG_WARNING(Logger::getInstance(), "[LuaScriptSystem] onUpdate error in '{}': {}", entry.scriptPath, err.what());
        }
    }
}

bool LuaScriptSystem::isLoaded(Entity entity) const
{
    return scripts.count(entity.id()) > 0;
}

} // namespace Faye
