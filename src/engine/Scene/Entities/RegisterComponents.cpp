#include "engine/Scene/Entities/RegisterComponents.hpp"
#include "Core/ECS/World.hpp"
#include "Renderer/PostProcess/PostProcessEffectLibrary.hpp"
#include "engine/Scene/Entities/Components.hpp"
#include "engine/Scene/Serialization/ComponentSerializers.hpp"
// The ONE translation unit that names kDescriptor<T>, and therefore the only
// engine .cpp that compiles <meta>. Everything downstream sees a plain
// `const TypeDescriptor *` on ComponentTypeInfo.
//
// -DFAYE_ENABLE_REFLECTION=OFF removes kDescriptor entirely, so every component
// must keep a working hand-written thunk pair until the off-switch is retired.
// That is the whole point of the escape hatch: a GCC reflection bug costs you
// this file, not the engine.
#if defined(FAYE_HAS_REFLECTION) && FAYE_HAS_REFLECTION
#include "Core/ECS/Reflection/Describe.hpp"
#endif

namespace Faye
{
    namespace
    {
        // Registers T reflected when reflection is available, falling back to
        // the hand-written pair otherwise. Deliberately local to this file: a
        // template needs to be visible where it is instantiated, and only the
        // registration TUs instantiate it. Putting it in a header would drag
        // <meta> along with it.
        template <class T, Ecs::Clone C = Ecs::Clone::copy>
        void registerComponent(Ecs::ComponentTypeRegistry &registry, const char *name,
                               void (*handWrittenSerialize)(const void *, Ecs::Serializer &),
                               void (*handWrittenDeserialize)(void *, Ecs::Deserializer &))
        {
#if defined(FAYE_HAS_REFLECTION) && FAYE_HAS_REFLECTION
            (void)handWrittenSerialize;
            (void)handWrittenDeserialize;
            registry.registerType<T, C>(name, &Ecs::kDescriptor<T>);
#else
            registry.registerType<T, C>(name, handWrittenSerialize, handWrittenDeserialize);
#endif
        }
    }

    void registerEngineComponents(Ecs::World &world)
    {
        // Registration order = inspector display order.
        //
        // To convert a component to reflection: swap registerType -> registerComponent.
        // The hand-written thunks stay as the reflection-off fallback and as the
        // reference the parity test in ReflectionTests.cpp checks against.
        auto &registry = world.types();
        registerComponent<TransformComponent>(registry,"Transform", Ecs::serializeTransform, Ecs::deserializeTransform);
        registerComponent<MeshRendererComponent>(registry,"Mesh", Ecs::serializeMesh, Ecs::deserializeMesh);
        registerComponent<CameraComponent, Ecs::Clone::skip>(registry,"Camera", Ecs::serializeCamera, Ecs::deserializeCamera);
        registerComponent<WaterComponent>(registry,"Water", Ecs::serializeWater, Ecs::deserializeWater);
        registerComponent<PointLightComponent>(registry,"Point Light", Ecs::serializePointLight, Ecs::deserializePointLight);
        registerComponent<DirectionalLightComponent>(registry,"Directional Light", Ecs::serializeDirectionalLight, Ecs::deserializeDirectionalLight);
        registerComponent<PostProcessStackComponent>(registry,"Post Process Stack", Ecs::serializePostProcessStack, Ecs::deserializePostProcessStack);
        registerComponent<RigidBody2dComponent>(registry,"RigidBody2D", Ecs::serializeRigidBody2d, Ecs::deserializeRigidBody2d);
        // EntityMetadata deliberately unregistered: the inspector draws the
        // name field itself, and it should not appear in the add/remove menu.
    }
}
