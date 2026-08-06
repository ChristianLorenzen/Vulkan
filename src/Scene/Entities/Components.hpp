#pragma once

#include <glm/glm.hpp>

#include <string>

// Headless by design: handles instead of registries, no renderer includes.
// The ECS core (Core/ECS/) must stay buildable without the renderer.
#include "Assets/ModelHandle.hpp"
#include "Core/ECS/Reflection/Annotations.hpp"
#include "Core/ECS/World.hpp"
#include "Renderer/Material/MaterialHandle.hpp"
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

    struct FAYE_ATTR(Ecs::TypeName("RigidBody2D")) RigidBody2dComponent
    {
        glm::vec2 velocity{};
        float mass{1.0f};
    };

    enum TestEnum {
        TESTONE,
        TESTTWO,
        FINAL
    };

    struct FAYE_ATTR(Ecs::TypeName("Transform")) TransformComponent
    {
        glm::vec3 translation{};
        glm::vec3 scale{1.0f, 1.0f, 1.0f};
        FAYE_ATTR(Ecs::Radians)           glm::vec3 rotation{};

        
        TestEnum enumVal{};
        glm::mat4 mat4() const;
        glm::mat3 normalMatrix() const;
    };

    struct FAYE_ATTR(Ecs::TypeName("Mesh")) MeshRendererComponent
    {
        ModelHandle modelHandle{};
        MaterialHandle materialHandle{};
        bool view = true;
    };

    struct FAYE_ATTR(Ecs::TypeName("Camera")) CameraComponent
    {
        FAYE_ATTR(Ecs::NotSerialized) Camera camera{};
        bool primary = false;
    };

    struct FAYE_ATTR(Ecs::TypeName("Point Light")) PointLightComponent
    {
        FAYE_ATTR(Ecs::ColorPicker)         glm::vec3 color{1.0f, 1.0f, 1.0f};
        FAYE_ATTR(Ecs::Range{0.0f, 100.0f}) float intensity = 1.0f;
        
        FAYE_ATTR(Ecs::Units("m")) 
        FAYE_ATTR(Ecs::Range{0.0f, 5.0f})         
        float radius = 0.25f;
    };

    /// A directional ("sun") light. Holds only radiometric properties; the
    /// light direction is the entity's forward vector, derived from the sibling
    /// TransformComponent.rotation (see LightingUniforms / packSceneLighting).
    /// This mirrors how PointLightComponent takes its position from the transform.
    struct FAYE_ATTR(Ecs::TypeName("Directional Light")) DirectionalLightComponent
    {
        FAYE_ATTR(Ecs::ColorPicker)         glm::vec3 color{1.0f, 1.0f, 1.0f};
        FAYE_ATTR(Ecs::Range{0.0f, 100.0f}) float intensity = 1.0f;
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
    struct FAYE_ATTR(Ecs::TypeName("Water")) WaterComponent
    {
        FAYE_ATTR(Ecs::Range{8, 256})       uint32_t subdivisions = 64;
    };
}
