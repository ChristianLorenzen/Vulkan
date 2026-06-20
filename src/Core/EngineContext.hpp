#pragma once

namespace Faye
{
    /// Per-tick context the engine pushes into every ITick system's OnUpdate
    /// (and, through the scripting systems, into all C++/Lua scripts).
    ///
    /// Carries only facts the engine decides for THIS tick and that a system
    /// cannot pull for itself. `dt` is the integration step for the current
    /// callback: the variable frame delta in OnUpdate, the fixed step in
    /// OnFixedUpdate. Ambient state (total elapsed time, etc.) is intentionally
    /// not here — query the clock for that.
    ///
    /// To expose a new engine variable to scripts:
    ///   1. Add a field here.
    ///   2. For Lua: add a sol::readonly() line in LuaScriptSystem::bindEngineAPI().
    ///   3. C++ scripts receive ctx by const-ref in onUpdate — no extra wiring needed.
    ///
    /// C++ access:   ctx.dt
    /// Lua access:   Engine.dt
    struct EngineContext
    {
        float dt = 0.0f;  ///< Integration step for the current tick (seconds).
    };
}
