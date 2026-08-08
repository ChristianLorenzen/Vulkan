#pragma once

#include <optional>
#include <glm/glm.hpp>

#include "engine/Assets/ModelRegistry.hpp"
#include "engine/Scene/Camera/Camera.hpp"
#include "engine/Scene/Scene.hpp"
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