#pragma once

#include <cstdint>
#include <unordered_map>

#include <glm/glm.hpp>

#include "Assets/ModelRegistry.hpp"
#include "Renderer/Scene/RenderScene.hpp"
#include "Scene/Scene.hpp"

namespace Faye
{
    struct RenderTransformHistoryEntry
    {
        glm::mat4 modelMatrix{1.0f};
        uint64_t lastSeenExtraction = 0;
    };

    struct RenderExtractionContext
    {
        const Scene &scene;
        ModelRegistry &modelRegistry;
        MaterialRegistry &materialRegistry;
        RenderSceneSnapshot &snapshot;
        const std::unordered_map<Scene::EntityId, RenderTransformHistoryEntry> &previousModelTransforms;
        uint64_t extractionIndex = 0;
    };
}