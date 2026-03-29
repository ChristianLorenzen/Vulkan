#pragma once

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
        const TransformComponent *transform = nullptr;
        Model *model = nullptr;
        glm::vec3 color{};
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