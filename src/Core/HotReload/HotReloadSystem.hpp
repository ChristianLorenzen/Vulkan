#pragma once

#include "Core/ITick.hpp"
#include "Core/HotReload/HotReloadManager.hpp"

namespace Faye
{
    /// Owns the engine's HotReloadManager and drives its lifecycle through the
    /// ITick contract:
    ///   OnInit     — register the watched directories
    ///   OnPostInit — start the watcher thread (after every other system has had
    ///                a chance to subscribe in its own OnInit)
    ///   OnUpdate   — dispatch queued filesystem events on the main thread
    ///   OnStop     — stop the watcher thread
    ///
    /// Subscribers (script .so reloads, shader recompiles, ...) attach their own
    /// callbacks via getHotReloadManager(); this system only owns the manager and
    /// its lifecycle, not what anyone does with the events.
    class HotReloadSystem : public ITick
    {
    public:
        HotReloadSystem() = default;

        void OnInit() override;
        void OnPostInit() override;
        void OnUpdate(const EngineContext &) override;
        void OnStop() override;

        HotReloadManager &getHotReloadManager() { return hotReloadManager; }

    private:
        HotReloadManager hotReloadManager;
    };
}
