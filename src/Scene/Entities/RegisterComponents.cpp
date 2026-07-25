#include "Scene/Entities/RegisterComponents.hpp"

#include "Core/ECS/World.hpp"
#include "Scene/Entities/Components.hpp"

namespace Faye
{
    void registerEngineComponents(Ecs::World &world)
    {
        // Registration order = inspector display order.
        auto &registry = world.types();
        registry.registerType<TransformComponent>("Transform");
        registry.registerType<MeshRendererComponent>("Mesh");
        registry.registerType<CameraComponent>("Camera");
        registry.registerType<WaterComponent>("Water");
        registry.registerType<PointLightComponent>("Point Light");
        registry.registerType<DirectionalLightComponent>("Directional Light");
        registry.registerType<PostProcessStackComponent>("Post Process Stack");
        registry.registerType<RigidBody2dComponent>("RigidBody2D");
        // EntityMetadata deliberately unregistered: the inspector draws the
        // name field itself, and it should not appear in the add/remove menu.
    }
}
