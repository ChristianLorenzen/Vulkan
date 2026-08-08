#pragma once

#include <string>

#include "Core/ECS/Entity.hpp"
#include "Core/EngineContext.hpp"
#include "Core/HotReload/HotReloadManager.hpp"
#include "engine/Scripting/IScript.hpp"
#include "engine/Scripting/ScriptComponents.hpp"
namespace Faye
{
    class Scene;
    class Entity;

    /// Loads native scripts (.so or built-in) and drives their lifecycle.
    /// Storage lives in the Scene's World as NativeScriptComponent; teardown
    /// runs through the World's remove hook installed by bindScene, so entity
    /// destruction — from gameplay, the editor, or scene teardown — unloads
    /// the script (onDestroy + destroy + dlclose) automatically.
    class ScriptSystem
    {
    public:
        ScriptSystem() = default;
        ~ScriptSystem() = default;   // teardown runs via unloadAll / remove hooks

        ScriptSystem(const ScriptSystem &) = delete;
        ScriptSystem &operator=(const ScriptSystem &) = delete;
        ScriptSystem(ScriptSystem &&) = delete;
        ScriptSystem &operator=(ScriptSystem &&) = delete;

        /// Bind the scene: registers NativeScriptComponent in the World's type
        /// registry and installs the teardown remove hook. Must run before any
        /// script is loaded, and this object must outlive attached scripts
        /// (unloadAll at shutdown guarantees that).
        void bindScene(Scene *scene);

        /// Load and start a script for the given entity from the .so path.
        /// Silently skips if the file does not exist.
        void loadScript(Entity entity, const std::string &soPath);

        /// Attach a pre-constructed built-in script instance to an entity.
        /// Ownership transfers to the component; teardown calls delete on the
        /// instance (no dlclose — libHandle stays null).
        void attachBuiltinScript(Entity entity, IScript *instance, const std::string &name);

        /// Stop and unload the script attached to an entity.
        void unloadScript(Entity entity);

        /// Unload then reload the script from the same .so path.
        void reloadScript(Entity entity);

        /// Unload every native script in the bound scene. Called at shutdown
        /// (ScriptingSystem::OnStop) so teardown runs while the scene, the
        /// World, and this system are all still alive.
        void unloadAll();

        /// Call onUpdate on all active scripts.
        void update(const EngineContext &ctx, Scene *scene);

        /// Register a hot-reload watch on bin/ for .so files.
        void registerHotReload(HotReloadManager &hotReloadManager);
        void unregisterHotReload();

    private:
        void teardown(Ecs::Entity entityHandle, NativeScriptComponent &script);

        Scene *boundScene = nullptr;
        HotReloadManager *hotReloadManagerPtr = nullptr;
        HotReloadManager::CallbackToken hotReloadToken = 0;
    };
}
