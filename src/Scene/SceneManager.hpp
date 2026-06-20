#pragma once

#include <memory>
#include <string>

#include "Core/ITick.hpp"
#include "Assets/ModelRegistry.hpp"
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

        RenderSceneSnapshot buildRenderSnapshot() { return renderExtractionManager.extract(getActiveScene(), modelRegistry, materialRegistry); }

    private:
        std::unique_ptr<Scene> activeScene;

        RenderExtractionManager renderExtractionManager;

        ModelRegistry &modelRegistry;
        MaterialRegistry &materialRegistry;
    };
}