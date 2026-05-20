#pragma once

#include <string>

#include "Scene/Entities/Entity.hpp"

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
        virtual void onUpdate(Entity entity, Scene *scene, float dt) { (void)entity; (void)scene; (void)dt; }
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
