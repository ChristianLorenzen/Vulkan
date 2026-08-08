#pragma once

#include <memory>
#include <string>

#include "Core/ITick.hpp"
#include "engine/Assets/ModelRegistry.hpp"
#include "Core/Jobs/JobSystem.hpp"
#include "Renderer/Material/MaterialRegistry.hpp"
#include "Renderer/Scene/RenderExtractionManager.hpp"
#include "Scene.hpp"
#include "engine/Scene/Serialization/SceneFileReader.hpp"
namespace Faye
{
    class ScriptSystem;
    class LuaScriptSystem;

    class SceneManager : public ITick
    {
    public:
        SceneManager(ModelRegistry &modelRegistry, MaterialRegistry &materialRegistry, AssetDatabase &assetDatabase);

        void OnInit() override;

        Scene &createScene(const std::string &sceneName = "Scene");
        bool hasActiveScene() const { return activeScene != nullptr; }

        Scene &getActiveScene();
        const Scene &getActiveScene() const;

        // Called after the scripting systems exist (registration order in
        // Engine::initialize). Needed for script re-attachment on scene load.
        void bindScriptEngines(ScriptSystem &scripts, LuaScriptSystem &luaScripts);

        // Save the active scene to a .faye file. Returns "" on success, else
        // an error message.
        std::string saveSceneToFile(const std::string &path);
        // Save As: writing an already-persisted scene to a DIFFERENT path
        // produces a copy, so the copy gets a fresh scene uuid. Without this
        // two .faye files claim the same identity (see sceneSaveNeedsNewUuid).
        std::string saveSceneAsToFile(const std::string &path);
        // Fill-in-place load: clears the active scene, then rebuilds it from
        // the file. result.error is set on failure.
        SceneFileLoadResult loadSceneFromFile(const std::string &path);

        const std::string &currentScenePath() const { return scenePath; }
        void clearScenePath() { scenePath.clear(); }

        const RenderSceneSnapshot &buildRenderSnapshot(Jobs::JobSystem &jobs) { return renderExtractionManager.extract(getActiveScene(), jobs); }

    private:
        std::unique_ptr<Scene> activeScene;
        std::string scenePath;

        // Declared before the extraction manager so the references it captures
        // are bound first.
        ModelRegistry &modelRegistry;
        MaterialRegistry &materialRegistry;
        AssetDatabase &assetDatabase;

        ScriptSystem *scriptSystem = nullptr;
        LuaScriptSystem *luaScriptSystem = nullptr;

        RenderExtractionManager renderExtractionManager;
    };
}