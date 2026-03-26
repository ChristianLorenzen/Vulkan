#pragma once

#include <vector>

#include "Renderer/Resources/Model.hpp"
#include "Scene/Camera/Camera.hpp"
#include "Scene/Entities/GameObject.hpp"
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

    struct RenderSceneSnapshot
    {
        const Camera *primaryCamera = nullptr;
        std::vector<RenderableInstance> renderables;
    };
}