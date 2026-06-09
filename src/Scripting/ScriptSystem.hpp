#pragma once

#include <string>
#include <unordered_map>

#include "Core/HotReload/HotReloadManager.hpp"
#include "Scene/Entities/EntityManager.hpp"
#include "Scripting/EngineContext.hpp"
#include "Scripting/IScript.hpp"

namespace Faye
{
    class Scene;
    class Entity;

    class ScriptSystem
    {
    public:
        ScriptSystem() = default;
        ~ScriptSystem();

        ScriptSystem(const ScriptSystem &) = delete;
        ScriptSystem &operator=(const ScriptSystem &) = delete;
        ScriptSystem(ScriptSystem &&) = delete;
        ScriptSystem &operator=(ScriptSystem &&) = delete;

        /// Bind the scene used for onStart / onDestroy calls during hot-reload.
        void bindScene(Scene *scene) { boundScene = scene; }

        /// Load and start a script for the given entity from the .so path.
        /// Silently skips if the file does not exist.
        void loadScript(Entity entity, const std::string &soPath);

        /// Attach a pre-constructed built-in script instance to an entity.
        /// The ScriptSystem takes ownership and will call delete on the instance
        /// when unloaded (no dlclose — libHandle stays null).
        void attachBuiltinScript(Entity entity, IScript *instance, const std::string &name);

        /// Stop and unload the script attached to an entity.
        void unloadScript(Entity entity);

        /// Unload then reload the script from the same .so path.
        void reloadScript(Entity entity);

        /// Call onUpdate on all active scripts.
        void update(const EngineContext &ctx, Scene *scene);

        /// Register a hot-reload watch on bin/ for .so files.
        void registerHotReload(HotReloadManager &hotReloadManager);
        void unregisterHotReload();

        /// Returns the ScriptComponent metadata for an entity, or nullptr if none loaded.
        const ScriptComponent *tryGetScriptComponent(EntityId entityId) const;

    private:
        struct LoadedScript
        {
            void *libHandle = nullptr;
            IScript *instance = nullptr;
            ScriptComponent component{};
        };

        void doUnload(EntityId entityId, LoadedScript &script);

        std::unordered_map<EntityId, LoadedScript> loadedScripts;
        Scene *boundScene = nullptr;
        HotReloadManager *hotReloadManagerPtr = nullptr;
        HotReloadManager::CallbackToken hotReloadToken = 0;
    };
}
