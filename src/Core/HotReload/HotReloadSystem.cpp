#include "Core/HotReload/HotReloadSystem.hpp"

#include "Core/Logging/Logger.hpp"
#include "Core/Path/Paths.hpp"
#include "quill/LogMacros.h"

namespace Faye
{
    void HotReloadSystem::OnInit()
    {
        LOG_INFO(Logger::get(), "HotReloadSystem OnInit");
        hotReloadManager.clearWatches();

        // TODO: Might want to split this into a helper file with a struct param
        hotReloadManager.addWatch({
            .id = "post-process-effects",
            .rootPath = Paths::assets() / "PostProcessEffects",
            .fileExtensions = {".ppfx"},
            .recursive = true,
        });
        hotReloadManager.addWatch({
            .id = "shader-sources",
            .rootPath = Paths::shaderSources(),
            .fileExtensions = {".vert", ".frag", ".comp"},
            .recursive = true,
        });
    }

    void HotReloadSystem::OnPostInit()
    {
        // Started in OnPostInit so every other system's OnInit has already had the
        // chance to subscribe before scan jobs begin delivering events.
        LOG_INFO(Logger::get(), "HotReloadSystem OnPostInit");
        hotReloadManager.start();
    }

    void HotReloadSystem::OnUpdate(const EngineContext &ctx)
    {
        if (ctx.jobs != nullptr)
        {
            hotReloadManager.tick(*ctx.jobs);
        }
        hotReloadManager.dispatchPendingEvents();
    }

    void HotReloadSystem::OnStop()
    {
        LOG_INFO(Logger::get(), "HotReloadSystem OnStop");
        hotReloadManager.stop();
    }
}
