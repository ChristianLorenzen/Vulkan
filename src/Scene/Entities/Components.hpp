#pragma once

#include <glm/glm.hpp>

#include <string>

// Headless by design: handles instead of registries, no renderer includes.
// The ECS core (Core/ECS/) must stay buildable without the renderer.
#include "Assets/ModelHandle.hpp"
#include "Renderer/Material/MaterialHandle.hpp"
#include "Renderer/PostProcess/PostProcessEffectLibrary.hpp"
#include "Scene/Camera/Camera.hpp"

namespace Faye
{
    /// Name (and future editor-only metadata) for an entity. Every entity the
    /// Scene creates carries one. Deliberately absent from the type registry:
    /// the inspector draws the name field itself.
    struct EntityMetadata
    {
        std::string name{};
    };

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

    /// A directional ("sun") light. Holds only radiometric properties; the
    /// light direction is the entity's forward vector, derived from the sibling
    /// TransformComponent.rotation (see LightingUniforms / packSceneLighting).
    /// This mirrors how PointLightComponent takes its position from the transform.
    struct DirectionalLightComponent
    {
        glm::vec3 color{1.0f, 1.0f, 1.0f};
        float intensity = 1.0f;
    };

    struct WaterState {
        float windSpeed;
        float windDir;
        float waveAmp;
        float waveChopiness;
    };

    struct WaterSample {
        float height;
        glm::vec3 normal;
        glm::vec2 velocity;
    };

    /// Marks an entity as a water surface and holds its mesh subdivision count.
    /// The WaterSubdivisionScript watches this component and rebuilds the mesh
    /// whenever subdivisions changes.
    struct WaterComponent
    {
        uint32_t subdivisions = 64;
    };
}