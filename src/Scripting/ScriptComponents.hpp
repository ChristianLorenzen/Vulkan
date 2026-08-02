#pragma once

#include <string>

namespace Faye
{
    class IScript;

    /// A native (.so / built-in) script attached to an entity. Stored in the
    /// Scene's World like any other component, so entity destruction sweeps it
    /// and the remove hook installed by ScriptSystem::bindScene runs the
    /// teardown (onDestroy, destroyScript/delete, dlclose) — the orphaned
    /// script leak is fixed by construction, not by remembering to erase.
    ///
    /// (LuaScriptComponent lives in LuaScriptSystem.hpp: it holds sol2 state
    /// references, and keeping it there spares every includer of this header
    /// the sol2 dependency.)
    struct NativeScriptComponent
    {
        std::string scriptPath;        // .so path, or "<builtin>" for attachBuiltinScript
        std::string scriptName;
        void *libHandle = nullptr;     // dlopen handle; null for built-ins
        IScript *instance = nullptr;   // created by the library's factory (or new)
    };
}
