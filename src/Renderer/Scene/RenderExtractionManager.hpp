#pragma once

#include <memory>
#include <vector>

#include "Assets/ModelRegistry.hpp"
#include "Renderer/Scene/RenderScene.hpp"
#include "Renderer/Scene/SceneRenderExtractor.hpp"
#include "Scene/Scene.hpp"

namespace Faye
{
    class RenderExtractionManager
    {
    public:
        RenderExtractionManager();

        RenderSceneSnapshot extract(const Scene &scene, ModelRegistry &modelRegistry) const;

    private:
        std::vector<std::unique_ptr<SceneRenderExtractor>> extractors;
    };
}