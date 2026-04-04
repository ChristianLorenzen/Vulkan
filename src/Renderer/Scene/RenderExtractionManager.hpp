#pragma once

#include <memory>
#include <unordered_map>
#include <vector>

#include "Assets/ModelRegistry.hpp"
#include "Renderer/Scene/RenderExtractionContext.hpp"
#include "Renderer/Scene/RenderScene.hpp"
#include "Renderer/Scene/SceneRenderExtractor.hpp"
#include "Scene/Scene.hpp"

namespace Faye
{
    class RenderExtractionManager
    {
    public:
        RenderExtractionManager();

        RenderSceneSnapshot extract(const Scene &scene, ModelRegistry &modelRegistry, MaterialRegistry &materialRegistry);

    private:
        std::vector<std::unique_ptr<SceneRenderExtractor>> extractors;
        std::unordered_map<Scene::EntityId, RenderTransformHistoryEntry> previousModelTransforms;
        uint64_t extractionIndex = 0;
    };
}