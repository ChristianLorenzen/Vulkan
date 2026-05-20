#pragma once

namespace Faye
{
    /// Engine-level variables available to ALL script types (C++ and Lua).
    ///
    /// To expose a new engine variable to scripts:
    ///   1. Add a field here.
    ///   2. For Lua: add a sol::readonly() line in LuaScriptSystem::bindEngineAPI().
    ///   3. C++ scripts receive ctx by const-ref in onUpdate — no extra wiring needed.
    ///
    /// C++ access:   ctx.dt,   ctx.time
    /// Lua access:   Engine.dt, Engine.time
    struct EngineContext
    {
        float dt   = 0.0f;  ///< Seconds elapsed since the previous frame (framerate-independent delta).
        float time = 0.0f;  ///< Total elapsed time in seconds since the engine started.
    };
}
