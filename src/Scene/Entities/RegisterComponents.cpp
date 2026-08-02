#include "Scene/Entities/RegisterComponents.hpp"

#include "Core/ECS/World.hpp"
#include "Scene/Entities/Components.hpp"
#include "Scene/Serialization/ComponentSerializers.hpp"

namespace Faye
{
    void registerEngineComponents(Ecs::World &world)
    {
        // Registration order = inspector display order.
        auto &registry = world.types();
        registry.registerType<TransformComponent>("Transform", Ecs::serializeTransform, Ecs::deserializeTransform);
        registry.registerType<MeshRendererComponent>("Mesh", Ecs::serializeMesh, Ecs::deserializeMesh);
        registry.registerType<CameraComponent, Ecs::Clone::skip>("Camera", Ecs::serializeCamera, Ecs::deserializeCamera);
        registry.registerType<WaterComponent>("Water", Ecs::serializeWater, Ecs::deserializeWater);
        registry.registerType<PointLightComponent>("Point Light", Ecs::serializePointLight, Ecs::deserializePointLight);
        registry.registerType<DirectionalLightComponent>("Directional Light", Ecs::serializeDirectionalLight, Ecs::deserializeDirectionalLight);
        registry.registerType<PostProcessStackComponent>("Post Process Stack", Ecs::serializePostProcessStack, Ecs::deserializePostProcessStack);
        registry.registerType<RigidBody2dComponent>("RigidBody2D", Ecs::serializeRigidBody2d, Ecs::deserializeRigidBody2d);
        // EntityMetadata deliberately unregistered: the inspector draws the
        // name field itself, and it should not appear in the add/remove menu.
    }
}
