#pragma once

#include <glm/glm.hpp>

#include <vector>

#include "Assets/ModelRegistry.hpp"
#include "Renderer/Material/MaterialRegistry.hpp"
#include "Renderer/PostProcess/PostProcessEffectLibrary.hpp"
#include "Scene/Camera/Camera.hpp"

namespace Faye
{
    struct RigidBody2dComponent
    {
        glm::vec2 velocity{};
        float mass{1.0f};
    };

    struct TransformComponent
    {
        glm::vec3 translation{};
        glm::vec3 scale{1.0f, 1.0f, 1.0f};
        glm::vec3 rotation{};

        glm::mat4 mat4() const;
        glm::mat3 normalMatrix() const;
    };

    struct MeshRendererComponent
    {
        ModelHandle modelHandle{};
        MaterialHandle materialHandle{};
    };

    struct CameraComponent
    {
        Camera camera{};
        bool primary = false;
    };

    struct PointLightComponent
    {
        glm::vec3 color{1.0f, 1.0f, 1.0f};
        float intensity = 1.0f;
        float radius = 0.25f;
    };

    /// Marks an entity as a water surface and holds its mesh subdivision count.
    /// The WaterSubdivisionScript watches this component and rebuilds the mesh
    /// whenever subdivisions changes.
    struct WaterComponent
    {
        uint32_t subdivisions = 64;
    };
}