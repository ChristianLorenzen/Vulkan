#pragma once

#include <glm/glm.hpp>

#include <vector>

#include "Renderer/Resources/Model.hpp"
#include "Scene/Camera/Camera.hpp"
#include "Scene/Entities/Components.hpp"
#include "Scene/Scene.hpp"

namespace Faye
{
    struct RenderableInstance
    {
        Scene::EntityId entity = Scene::invalidEntity;
        glm::mat4 modelMatrix{1.0f};
        glm::mat4 priorModelMatrix{1.0f};
        Model *model = nullptr;
        const MaterialRegistry *materialRegistry = nullptr;
        MaterialHandle materialHandle{};
        Material *material = nullptr;
    };

    struct PointLightInstance
    {
        Scene::EntityId entity = Scene::invalidEntity;
        const TransformComponent *transform = nullptr;
        glm::vec3 color{1.0f};
        float intensity = 1.0f;
        float radius = 0.25f;
    };

    struct RenderSceneSnapshot
    {
        const Camera *primaryCamera = nullptr;
        std::vector<RenderableInstance> renderables;
        std::vector<PointLightInstance> pointLights;
    };
}