#include "engine/Scripting/ScriptSystem.hpp"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// Map POSIX dl* API to Win32 equivalents so the rest of this file is unchanged.
static void *dl_open(const char *path) { return reinterpret_cast<void *>(LoadLibraryA(path)); }
static int dl_close(void *h) { return FreeLibrary(reinterpret_cast<HMODULE>(h)) ? 0 : -1; }
static void *dl_sym(void *h, const char *sym) { return reinterpret_cast<void *>(GetProcAddress(reinterpret_cast<HMODULE>(h), sym)); }
static const char *dl_error()
{
    DWORD err = GetLastError();
    if (err == 0)
        return nullptr;
    static char buf[512];
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                   nullptr, err, 0, buf, sizeof(buf), nullptr);
    return buf;
}

// Macro arguments not used in the replacement body are discarded by the
// preprocessor — RTLD_NOW/RTLD_LOCAL are never seen by the compiler.
#define dlopen(path, flags) dl_open(path)
#define dlclose(h) dl_close(h)
#define dlsym(h, sym) dl_sym(h, sym)
#define dlerror() dl_error()
#define SHARED_LIB_EXT ".dll"
#else
#include <dlfcn.h>
#define SHARED_LIB_EXT ".so"
#endif

#include <filesystem>
#include <string>
#include <vector>

#include "Core/Logging/Logger.hpp"
#include "Core/Path/Paths.hpp"
#include "engine/Scene/Entities/Entity.hpp"
#include "engine/Scene/Scene.hpp"
#include "engine/Scene/Serialization/ComponentSerializers.hpp"
#include "quill/LogMacros.h"

using namespace Faye;

namespace
{
    constexpr const char *kBuiltinPath = "<builtin>";
}

void ScriptSystem::bindScene(Scene *scene)
{
    boundScene = scene;
    if (scene == nullptr)
    {
        return;
    }

    auto &world = scene->getWorld();

world.types().registerType<NativeScriptComponent, Ecs::Clone::skip>(
            "Native Script", Ecs::serializeNativeScript, Ecs::deserializeNativeScript);
    // The channel that fixes the orphaned-script leak: every removal route —
    // unloadScript, entity destruction, the editor's type-registry remove,
    // scene teardown — funnels through this hook while the data is intact.
    world.setRemoveHook<NativeScriptComponent>(
        [this](Ecs::World &, Ecs::Entity entityHandle, void *raw)
        {
            teardown(entityHandle, *static_cast<NativeScriptComponent *>(raw));
        });
}

void ScriptSystem::teardown(Ecs::Entity entityHandle, NativeScriptComponent &script)
{
    if (script.instance != nullptr)
    {
        if (boundScene != nullptr)
        {
            script.instance->onDestroy(Entity{boundScene, entityHandle}, boundScene);
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
        else
        {
            // Built-in script: no shared library, owned directly via new/delete.
            delete script.instance;
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
    if (boundScene == nullptr || !entity.isValid())
    {
        LOG_WARNING(Logger::get(), "ScriptSystem: loadScript without a bound scene / valid entity");
        return;
    }

    // The component (and therefore the scene file) keeps the project-relative
    // path; dlopen and the existence check need the absolute one. A stored
    // "bin/libfoo.so" resolved against the working directory only worked when
    // the app happened to be launched from the repo root.
    const std::string scriptPath = Paths::toProjectRelative(soPath);
    const std::string resolvedPath = Paths::fromProjectRelative(scriptPath);

    if (!std::filesystem::exists(resolvedPath))
    {
        LOG_WARNING(Logger::get(), "ScriptSystem: .so not found, skipping: {}", resolvedPath);
        return;
    }

    unloadScript(entity);   // replace any existing script (hook tears it down)

    void *handle = dlopen(resolvedPath.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (handle == nullptr)
    {
        LOG_ERROR(Logger::get(), "ScriptSystem: dlopen failed for '{}': {}", resolvedPath, dlerror());
        return;
    }

    // Clear any stale dlerror state before querying.
    dlerror();
    auto *createFn = reinterpret_cast<CreateScriptFn>(dlsym(handle, "createScript"));
    const char *symErr = dlerror();
    if (symErr != nullptr || createFn == nullptr)
    {
        LOG_ERROR(Logger::get(), "ScriptSystem: 'createScript' not found in '{}': {}",
                  resolvedPath, symErr != nullptr ? symErr : "null symbol");
        dlclose(handle);
        return;
    }

    IScript *instance = createFn();
    if (instance == nullptr)
    {
        LOG_ERROR(Logger::get(), "ScriptSystem: createScript() returned nullptr for '{}'", resolvedPath);
        dlclose(handle);
        return;
    }

    const std::string scriptName = std::filesystem::path(scriptPath).stem().string();

    // Component first, then onStart: if onStart destroys its own entity, the
    // hook already owns cleanup and the script still sees a full lifecycle.
    boundScene->getWorld().add<NativeScriptComponent>(
        entity.handle(), NativeScriptComponent{scriptPath, scriptName, handle, instance});
    instance->onStart(entity, boundScene);

    LOG_INFO(Logger::get(), "ScriptSystem: loaded '{}' for entity {}", scriptName, entity.handle().index);
}

void ScriptSystem::attachBuiltinScript(Entity entity, IScript *instance, const std::string &name)
{
    if (instance == nullptr)
        return;

    if (boundScene == nullptr || !entity.isValid())
    {
        LOG_WARNING(Logger::get(), "ScriptSystem: attachBuiltinScript without a bound scene / valid entity");
        delete instance;
        return;
    }

    unloadScript(entity);   // replace any existing script (hook tears it down)

    boundScene->getWorld().add<NativeScriptComponent>(
        entity.handle(), NativeScriptComponent{kBuiltinPath, name, nullptr, instance});
    instance->onStart(entity, boundScene);

    LOG_INFO(Logger::get(), "ScriptSystem: attached built-in '{}' for entity {}", name, entity.handle().index);
}

void ScriptSystem::unloadScript(Entity entity)
{
    if (boundScene == nullptr)
        return;

    auto &world = boundScene->getWorld();
    if (world.has<NativeScriptComponent>(entity.handle()))
    {
        world.remove<NativeScriptComponent>(entity.handle());   // hook runs teardown
        LOG_INFO(Logger::get(), "ScriptSystem: unloaded script for entity {}", entity.handle().index);
    }
}

void ScriptSystem::reloadScript(Entity entity)
{
    if (boundScene == nullptr)
        return;

    auto *script = boundScene->getWorld().tryGet<NativeScriptComponent>(entity.handle());
    if (script == nullptr)
    {
        LOG_WARNING(Logger::get(),
                    "ScriptSystem: reloadScript called for entity {} but no script loaded",
                    entity.handle().index);
        return;
    }

    if (script->scriptPath == kBuiltinPath)
    {
        LOG_WARNING(Logger::get(), "ScriptSystem: built-in script '{}' cannot be reloaded", script->scriptName);
        return;
    }

    const std::string soPath = script->scriptPath;
    unloadScript(entity);
    loadScript(entity, soPath);
}

void ScriptSystem::unloadAll()
{
    if (boundScene == nullptr)
        return;

    auto &world = boundScene->getWorld();
    auto *pool = world.poolIfExists<NativeScriptComponent>();
    if (pool == nullptr)
        return;

    // Snapshot: each remove mutates the pool we'd otherwise be iterating.
    std::vector<Ecs::Entity> attached;
    attached.reserve(pool->set.size());
    for (const uint32_t entityIndex : pool->set.entities())
        attached.push_back(world.entityAt(entityIndex));

    for (const Ecs::Entity entityHandle : attached)
        world.remove<NativeScriptComponent>(entityHandle);
}

void ScriptSystem::update(const EngineContext &ctx, Scene *scene)
{
    if (scene == nullptr)
    {
        return;
    }

    auto &world = scene->getWorld();
    auto *pool = world.poolIfExists<NativeScriptComponent>();
    if (pool == nullptr)
        return;

    // Snapshot the attached entities first: scripts may make structural
    // changes (destroy entities, load/unload scripts) from onUpdate, which
    // would invalidate a live iteration of the pool.
    std::vector<Ecs::Entity> attached;
    attached.reserve(pool->set.size());
    for (const uint32_t entityIndex : pool->set.entities())
        attached.push_back(world.entityAt(entityIndex));

    for (const Ecs::Entity entityHandle : attached)
    {
        if (!world.alive(entityHandle))
            continue;
        auto *script = world.tryGet<NativeScriptComponent>(entityHandle);
        if (script == nullptr || script->instance == nullptr)
            continue;

        script->instance->onUpdate(Entity{scene, entityHandle}, scene, ctx);
    }
}

void ScriptSystem::registerHotReload(HotReloadManager &hotReloadManager)
{
    hotReloadManagerPtr = &hotReloadManager;

    hotReloadManager.addWatch({
        .id = "script-libs",
        .rootPath = Paths::bin(),
        .fileExtensions = {SHARED_LIB_EXT},
        .recursive = false,
    });

    hotReloadToken = hotReloadManager.subscribe(
        [this](const HotReloadEvent &event)
        {
            if (event.watchId != "script-libs" || event.type != HotReloadEventType::Modified)
            {
                return;
            }

            if (boundScene == nullptr)
                return;

            // Both sides compared in the project-relative form the components
            // store; the watcher reports absolute paths.
            const std::string changedPath = Paths::toProjectRelative(event.path.string());

            // Snapshot: reloadScript restructures the pool mid-iteration.
            auto &world = boundScene->getWorld();
            auto *pool = world.poolIfExists<NativeScriptComponent>();
            if (pool == nullptr)
                return;

            std::vector<Ecs::Entity> toReload;
            for (const uint32_t entityIndex : pool->set.entities())
            {
                const Ecs::Entity entityHandle = world.entityAt(entityIndex);
                const auto *script = world.tryGet<NativeScriptComponent>(entityHandle);
                const std::string scriptPath = Paths::toProjectRelative(script->scriptPath);
                if (scriptPath == changedPath)
                {
                    toReload.push_back(entityHandle);
                }
            }

            for (const Ecs::Entity entityHandle : toReload)
            {
                LOG_INFO(Logger::get(),
                         "ScriptSystem: hot-reloading '{}' for entity {}",
                         changedPath, entityHandle.index);
                reloadScript(Entity{boundScene, entityHandle});
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
