#pragma once

#include <sol/sol.hpp>
#include <string>

#include "Core/EngineContext.hpp"

namespace Faye
{
    class Entity;
    class Scene;

    /// A Lua script attached to an entity. Stored in the Scene's World; the
    /// remove hook installed by LuaScriptSystem::bindScene calls onDestroy and
    /// drops the sol references so the Lua GC can collect the sandbox — entity
    /// destruction can no longer orphan a script.
    ///
    /// ⚠️ sol::state is not thread-safe: everything touching this component
    /// stays on the main thread (a needsMainThread system once Phase 7 lands).
    struct LuaScriptComponent
    {
        std::string scriptPath;
        sol::environment environment;      // per-script sandbox in the shared state
        sol::safe_function onUpdate;       // cached lookups
        sol::safe_function onDestroy;
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

        /// Bind the scene: registers LuaScriptComponent in the World's type
        /// registry and installs the teardown remove hook. Must run before any
        /// script is loaded. unloadAll must run before this object dies —
        /// components hold references into this object's sol::state.
        void bindScene(Scene *scene);

        /// Register the Faye engine API (Entity, Scene, etc.) into the Lua state.
        void bindEngineAPI();

        /// Load a Lua script file and bind it to an entity.
        /// Silently skips if the file does not exist.
        void loadScript(Entity entity, const std::string &scriptPath, Scene *scene);

        /// Unload the script bound to an entity (calls onDestroy if present).
        void unloadScript(Entity entity, Scene *scene);

        /// Reload the script from the same path (unload + load).
        void reloadScript(Entity entity, Scene *scene);

        /// Unload every Lua script in the bound scene. Called at shutdown so
        /// sol references die while the state and the World are both alive.
        void unloadAll();

        /// Call onUpdate on all active scripts.
        void update(const EngineContext &ctx, Scene *scene);

        bool isLoaded(Entity entity) const;

    private:
        sol::state lua;
        Scene *boundScene = nullptr;

        /// Holds the engine-level variables that are pushed into lua["Engine"] each frame.
        EngineContext engineContext;
    };
}
