#pragma once

#include <memory>
#include <string>

#include "Core/ITick.hpp"
#include "Assets/ModelRegistry.hpp"
#include "Core/Jobs/JobSystem.hpp"
#include "Renderer/Material/MaterialRegistry.hpp"
#include "Renderer/Scene/RenderExtractionManager.hpp"
#include "Scene.hpp"

namespace Faye
{
    class SceneManager : public ITick
    {
    public:
        SceneManager(ModelRegistry &modelRegistry, MaterialRegistry &materialRegistry);

        void OnInit() override;

        Scene &createScene(const std::string &sceneName = "Scene");
        bool hasActiveScene() const { return activeScene != nullptr; }

        Scene &getActiveScene();
        const Scene &getActiveScene() const;

        const RenderSceneSnapshot &buildRenderSnapshot(Jobs::JobSystem &jobs) { return renderExtractionManager.extract(getActiveScene(), jobs); }

    private:
        std::unique_ptr<Scene> activeScene;

        // Declared before the extraction manager so the references it captures
        // are bound first.
        ModelRegistry &modelRegistry;
        MaterialRegistry &materialRegistry;

        RenderExtractionManager renderExtractionManager;
    };
}