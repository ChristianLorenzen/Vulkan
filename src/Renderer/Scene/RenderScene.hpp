#pragma once

#include <glm/glm.hpp>

#include <vector>

#include "Core/ECS/Entity.hpp"
#include "Renderer/Material/MaterialRegistry.hpp"
#include "Renderer/Resources/Model.hpp"
#include "Scene/Camera/Camera.hpp"
#include "Scene/Entities/Components.hpp"
#include "Scene/Scene.hpp"

namespace Faye
{
    struct RenderableInstance
    {
        Ecs::Entity entity{};
        glm::mat4 modelMatrix{1.0f};
        glm::mat4 priorModelMatrix{1.0f};
        Model *model = nullptr;
        const MaterialRegistry *materialRegistry = nullptr;
        MaterialHandle materialHandle{};
        Material *material = nullptr;
    };

    struct PointLightInstance
    {
        Ecs::Entity entity{};
        const TransformComponent *transform = nullptr;
        glm::vec3 color{1.0f};
        float intensity = 1.0f;
        float radius = 0.25f;
    };

    struct DirectionalLightInstance
    {
        Ecs::Entity entity{};
        // Direction is derived from this transform's rotation during packing.
        const TransformComponent *transform = nullptr;
        glm::vec3 color{1.0f};
        float intensity = 1.0f;
    };

    // Motion-vector history, stored as an ECS component so its lifetime is
    // World-managed: World::destroy sweeps every pool, so the entry vanishes
    // with the entity. Written back (single-threaded) after each frame's
    // extraction; read next frame to supply the prior model matrix.
    struct PreviousTransformComponent
    {
        glm::mat4 modelMatrix{1.0f};
        uint64_t lastSeenExtraction = 0;
    };

    struct RenderSceneSnapshot
    {
        const Camera *primaryCamera = nullptr;
        const SceneSettings *sceneSettings = nullptr;
        std::vector<RenderableInstance> renderables;
        std::vector<PointLightInstance> pointLights;
        std::vector<DirectionalLightInstance> directionalLights;
    };
}