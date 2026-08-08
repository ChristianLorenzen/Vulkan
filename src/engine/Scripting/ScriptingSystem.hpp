#pragma once

#include "Core/ITick.hpp"
#include "engine/Scripting/ScriptSystem.hpp"
#include "engine/Scripting/LuaScriptSystem.hpp"
namespace Faye
{
    class SceneManager;
    class HotReloadManager;



    class ScriptingSystem : public ITick
    {
    public:
        ScriptingSystem(SceneManager &sceneManager, HotReloadManager &hotReloadManager);

        ///   OnInit     — register the native script-reload watch
        ///   OnPostInit — bind the active scene + expose the Lua engine API
        ///   OnUpdate   — forward the per-tick EngineContext to both engines
        ///   OnStop     — tear down the script-reload watch
        void OnInit() override;
        void OnPostInit() override;
        void OnUpdate(const EngineContext &ctx) override;
        void OnStop() override;

        ScriptSystem &getScriptSystem() { return scriptSystem; }
        LuaScriptSystem &getLuaScriptSystem() { return luaScriptSystem; }

    private:
        ScriptSystem scriptSystem;
        LuaScriptSystem luaScriptSystem;
        SceneManager &sceneManager;
        HotReloadManager &hotReloadManager;
    };
}
