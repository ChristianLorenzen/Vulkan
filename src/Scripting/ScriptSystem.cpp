#include "Scripting/ScriptSystem.hpp"

#include <dlfcn.h>
#include <filesystem>
#include <string>

#include "Core/Logging/Logger.hpp"
#include "Scene/Entities/Entity.hpp"
#include "Scene/Scene.hpp"
#include "quill/LogMacros.h"

using namespace Faye;

ScriptSystem::~ScriptSystem()
{
    // Cleanup without calling onDestroy – the scene may already be gone.
    for (auto &[entityId, script] : loadedScripts)
    {
        if (script.libHandle != nullptr && script.instance != nullptr)
        {
            auto *destroyFn = reinterpret_cast<DestroyScriptFn>(
                dlsym(script.libHandle, "destroyScript"));
            if (destroyFn != nullptr)
            {
                destroyFn(script.instance);
            }
            script.instance = nullptr;
        }
        if (script.libHandle != nullptr)
        {
            dlclose(script.libHandle);
            script.libHandle = nullptr;
        }
    }
    loadedScripts.clear();
}

void ScriptSystem::doUnload(EntityId entityId, LoadedScript &script)
{
    if (script.instance != nullptr)
    {
        if (boundScene != nullptr)
        {
            Entity entity = boundScene->getEntity(entityId);
            script.instance->onDestroy(entity, boundScene);
        }

        if (script.libHandle != nullptr)
        {
            auto *destroyFn = reinterpret_cast<DestroyScriptFn>(
                dlsym(script.libHandle, "destroyScript"));
            if (destroyFn != nullptr)
            {
                destroyFn(script.instance);
            }
        }
        script.instance = nullptr;
    }

    if (script.libHandle != nullptr)
    {
        dlclose(script.libHandle);
        script.libHandle = nullptr;
    }
}

void ScriptSystem::loadScript(Entity entity, const std::string &soPath)
{
    const EntityId entityId = entity.id();

    if (!std::filesystem::exists(soPath))
    {
        LOG_WARNING(Logger::getInstance(), "ScriptSystem: .so not found, skipping: {}", soPath);
        return;
    }

    // Unload any existing script on this entity first.
    auto it = loadedScripts.find(entityId);
    if (it != loadedScripts.end())
    {
        doUnload(entityId, it->second);
        loadedScripts.erase(it);
    }

    void *handle = dlopen(soPath.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (handle == nullptr)
    {
        LOG_ERROR(Logger::getInstance(), "ScriptSystem: dlopen failed for '{}': {}", soPath, dlerror());
        return;
    }

    // Clear any stale dlerror state before querying.
    dlerror();
    auto *createFn = reinterpret_cast<CreateScriptFn>(dlsym(handle, "createScript"));
    const char *symErr = dlerror();
    if (symErr != nullptr || createFn == nullptr)
    {
        LOG_ERROR(Logger::getInstance(), "ScriptSystem: 'createScript' not found in '{}': {}",
                  soPath, symErr != nullptr ? symErr : "null symbol");
        dlclose(handle);
        return;
    }

    IScript *instance = createFn();
    if (instance == nullptr)
    {
        LOG_ERROR(Logger::getInstance(), "ScriptSystem: createScript() returned nullptr for '{}'", soPath);
        dlclose(handle);
        return;
    }

    const std::string scriptName = std::filesystem::path(soPath).stem().string();

    if (boundScene != nullptr)
    {
        instance->onStart(entity, boundScene);
    }

    loadedScripts[entityId] = LoadedScript{
        .libHandle = handle,
        .instance  = instance,
        .component = ScriptComponent{soPath, scriptName},
    };

    LOG_INFO(Logger::getInstance(), "ScriptSystem: loaded '{}' for entity {}", scriptName, entityId);
}

void ScriptSystem::unloadScript(Entity entity)
{
    const EntityId entityId = entity.id();
    auto it = loadedScripts.find(entityId);
    if (it == loadedScripts.end())
    {
        return;
    }

    doUnload(entityId, it->second);
    loadedScripts.erase(it);
    LOG_INFO(Logger::getInstance(), "ScriptSystem: unloaded script for entity {}", entityId);
}

void ScriptSystem::reloadScript(Entity entity)
{
    const EntityId entityId = entity.id();
    auto it = loadedScripts.find(entityId);
    if (it == loadedScripts.end())
    {
        LOG_WARNING(Logger::getInstance(),
                    "ScriptSystem: reloadScript called for entity {} but no script loaded", entityId);
        return;
    }

    const std::string soPath = it->second.component.scriptPath;
    doUnload(entityId, it->second);
    loadedScripts.erase(it);

    loadScript(entity, soPath);
}

void ScriptSystem::update(const EngineContext &ctx, Scene *scene)
{
    if (scene == nullptr)
    {
        return;
    }

    for (auto &[entityId, script] : loadedScripts)
    {
        if (script.instance == nullptr)
        {
            continue;
        }

        Entity entity = scene->getEntity(entityId);
        if (!entity.isValid())
        {
            continue;
        }

        script.instance->onUpdate(entity, scene, ctx);
    }
}

void ScriptSystem::registerHotReload(HotReloadManager &hotReloadManager)
{
    hotReloadManagerPtr = &hotReloadManager;

    hotReloadManager.addWatch({
        .id             = "script-libs",
        .rootPath       = "./bin",
        .fileExtensions = {".so"},
        .recursive      = false,
    });

    hotReloadToken = hotReloadManager.subscribe(
        [this](const HotReloadEvent &event)
        {
            if (event.watchId != "script-libs" || event.type != HotReloadEventType::Modified)
            {
                return;
            }

            const std::string changedPath =
                std::filesystem::path(event.path).lexically_normal().string();

            // Iterate a snapshot to avoid invalidating the map during reload.
            std::vector<EntityId> toReload;
            for (const auto &[entityId, script] : loadedScripts)
            {
                const std::string scriptPath =
                    std::filesystem::path(script.component.scriptPath).lexically_normal().string();
                if (scriptPath == changedPath)
                {
                    toReload.push_back(entityId);
                }
            }

            for (EntityId entityId : toReload)
            {
                if (boundScene != nullptr)
                {
                    Entity entity = boundScene->getEntity(entityId);
                    LOG_INFO(Logger::getInstance(),
                             "ScriptSystem: hot-reloading '{}' for entity {}",
                             changedPath, entityId);
                    reloadScript(entity);
                }
            }
        },
        std::vector<std::string_view>{"script-libs"});
}

void ScriptSystem::unregisterHotReload()
{
    if (hotReloadManagerPtr != nullptr && hotReloadToken != 0)
    {
        hotReloadManagerPtr->unsubscribe(hotReloadToken);
        hotReloadToken = 0;
    }
    hotReloadManagerPtr = nullptr;
}

const ScriptComponent *ScriptSystem::tryGetScriptComponent(EntityId entityId) const
{
    auto it = loadedScripts.find(entityId);
    if (it == loadedScripts.end())
    {
        return nullptr;
    }
    return &it->second.component;
}
