#pragma once

namespace Faye
{
    /// Engine-level variables exposed to Lua scripts via the global 'Engine' table.
    ///
    /// Add new fields here to expand what scripts can read from Lua without
    /// modifying any other file. After adding a field, register it in
    /// LuaScriptSystem::bindEngineAPI() with sol::readonly().
    ///
    /// Lua usage:
    ///   Engine.dt    -- seconds since last frame  (framerate-independent timing)
    ///   Engine.time  -- total elapsed time in seconds
    struct LuaEngineContext
    {
        float dt   = 0.0f;  ///< Delta time: seconds elapsed since the previous frame.
        float time = 0.0f;  ///< Total elapsed time in seconds since the engine started.
    };
}
