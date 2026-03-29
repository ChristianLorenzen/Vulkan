#pragma once

#include <optional>

#include "Assets/ModelRegistry.hpp"
#include "Scene/Scene.hpp"

namespace Faye
{
    struct SceneRaycastHit
    {
        Scene::EntityId entity = Scene::invalidEntity;
        float distance = 0.0f;
        glm::vec3 position{0.0f};
    };

    std::optional<SceneRaycastHit> raycastScene(
        const Scene &scene,
        const ModelRegistry &modelRegistry,
        const Camera &camera,
        const glm::vec2 &viewportUv);
}