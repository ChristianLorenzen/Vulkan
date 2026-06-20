#pragma once

#include <string>

#include "Scene/Entities/Entity.hpp"
#include "Core/EngineContext.hpp"

namespace Faye
{
    class Scene;

    /// Abstract base class for all user scripts.
    /// Concrete scripts are compiled as shared libraries (.so) and loaded at runtime.
    class IScript
    {
    public:
        virtual ~IScript() = default;

        virtual void onStart(Entity entity, Scene *scene) { (void)entity; (void)scene; }
        /// ctx.dt — seconds since last frame (use for framerate-independent movement)
        virtual void onUpdate(Entity entity, Scene *scene, const EngineContext &ctx) { (void)entity; (void)scene; (void)ctx; }
        virtual void onDestroy(Entity entity, Scene *scene) { (void)entity; (void)scene; }
    };

    /// Lightweight metadata stored by ScriptSystem, keyed to an entity.
    struct ScriptComponent
    {
        std::string scriptPath;
        std::string scriptName;
    };
}

// Function-pointer types for the C-linkage factory pair that every script .so must export:
//   extern "C" Faye::IScript *createScript();
//   extern "C" void destroyScript(Faye::IScript *);
using CreateScriptFn  = Faye::IScript *(*)();
using DestroyScriptFn = void (*)(Faye::IScript *);

// FAYE_REGISTER_SCRIPT(ClassName)
// Place this macro once at file scope in a script .cpp to generate the required
// extern "C" factory functions automatically — no manual boilerplate needed.
#define FAYE_REGISTER_SCRIPT(ClassName)                          \
    extern "C" Faye::IScript *createScript()                     \
    {                                                            \
        return new ClassName();                                  \
    }                                                            \
    extern "C" void destroyScript(Faye::IScript *script)         \
    {                                                            \
        delete script;                                           \
    }
