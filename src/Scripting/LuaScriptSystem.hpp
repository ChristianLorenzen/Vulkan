#pragma once

#include <sol/sol.hpp>
#include <string>
#include <unordered_map>

#include "Scene/Entities/Entity.hpp"
#include "Core/EngineContext.hpp"

namespace Faye
{
    class Scene;

    /// Metadata stored on an entity to track which Lua script is bound to it.
    struct LuaScriptComponent
    {
        std::string scriptPath;
    };

    /// Manages per-entity Lua scripts loaded via sol2/Lua 5.4.
    /// Scripts must define optional global functions onStart(entity),
    /// onUpdate(entity, dt), and onDestroy(entity) in their own sandboxed env.
    class LuaScriptSystem
    {
    public:
        LuaScriptSystem();
        ~LuaScriptSystem() = default;

        LuaScriptSystem(const LuaScriptSystem &) = delete;
        LuaScriptSystem &operator=(const LuaScriptSystem &) = delete;

        /// Register the Faye engine API (Entity, Scene, etc.) into the Lua state.
        void bindEngineAPI();

        /// Load a Lua script file and bind it to an entity.
        /// Silently skips if the file does not exist.
        void loadScript(Entity entity, const std::string &scriptPath, Scene *scene);

        /// Unload the script bound to an entity (calls onDestroy if present).
        void unloadScript(Entity entity, Scene *scene);

        /// Reload the script from the same path (unload + load).
        void reloadScript(Entity entity, Scene *scene);

        /// Call onUpdate on all active scripts.
        void update(const EngineContext &ctx, Scene *scene);

        bool isLoaded(Entity entity) const;

    private:
        sol::state lua;

        struct LuaScriptEntry
        {
            std::string scriptPath;
            sol::environment scriptEnv;
            sol::safe_function onUpdate;
            sol::safe_function onDestroy;
        };

        std::unordered_map<uint32_t, LuaScriptEntry> scripts; // entityId → entry

        /// Holds the engine-level variables that are pushed into lua["Engine"] each frame.
        EngineContext engineContext;
    };
}
