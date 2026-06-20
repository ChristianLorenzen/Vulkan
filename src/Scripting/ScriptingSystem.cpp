#include "Scripting/ScriptingSystem.hpp"

#include "Scene/SceneManager.hpp"
#include "Core/Logging/Logger.hpp"
#include "quill/LogMacros.h"

namespace Faye
{
    ScriptingSystem::ScriptingSystem(SceneManager &sceneManager, HotReloadManager &hotReloadManager)
        : sceneManager(sceneManager), hotReloadManager(hotReloadManager) {}

    void ScriptingSystem::OnInit()
    {
        LOG_INFO(Logger::get(), "ScriptingSystem OnInit");
        scriptSystem.registerHotReload(hotReloadManager);
    }

    void ScriptingSystem::OnPostInit()
    {
        LOG_INFO(Logger::get(), "ScriptingSystem OnPostInit");
        // The scene was created in SceneManager::OnInit, which runs before any
        // OnPostInit, so the active scene is guaranteed to exist here.
        scriptSystem.bindScene(&sceneManager.getActiveScene());
        luaScriptSystem.bindEngineAPI();
    }

    void ScriptingSystem::OnUpdate(const EngineContext &ctx)
    {
        if (!sceneManager.hasActiveScene())
            return;

        Scene &scene = sceneManager.getActiveScene();
        scriptSystem.update(ctx, &scene);
        luaScriptSystem.update(ctx, &scene);
    }

    void ScriptingSystem::OnStop()
    {
        LOG_INFO(Logger::get(), "ScriptingSystem OnStop");
        scriptSystem.unregisterHotReload();
    }
}
