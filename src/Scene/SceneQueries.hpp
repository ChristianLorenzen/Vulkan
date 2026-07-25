#pragma once

#include <optional>
#include <glm/glm.hpp>

#include "Assets/ModelRegistry.hpp"
#include "Scene/Camera/Camera.hpp"
#include "Scene/Scene.hpp"

namespace Faye
{
    struct SceneRaycastHit
    {
        Ecs::Entity entity{};
        float distance = 0.0f;
        glm::vec3 position{0.0f};
    };

    std::optional<SceneRaycastHit> raycastScene(
        const Scene &scene,
        const ModelRegistry &modelRegistry,
        const Camera &camera,
        const glm::vec2 &viewportUv);
}