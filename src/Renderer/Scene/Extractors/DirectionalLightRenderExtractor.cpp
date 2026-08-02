#include "Renderer/Scene/Extractors/DirectionalLightRenderExtractor.hpp"

#include "Core/ECS/World.hpp"

namespace Faye
{
    Ecs::SystemAccess DirectionalLightRenderExtractor::access() const
    {
        return Ecs::SystemAccess{}
            .read<TransformComponent>()
            .read<DirectionalLightComponent>();
    }

    void DirectionalLightRenderExtractor::run(Ecs::World &world, const EngineContext &,
                                              Jobs::JobSystem &, Ecs::CommandBuffer &)
    {
        world.view<const TransformComponent, const DirectionalLightComponent>().each(
            [&](Ecs::Entity entity, const TransformComponent &transform, const DirectionalLightComponent &light)
            {
                snapshot.directionalLights.push_back(DirectionalLightInstance{
                    entity,
                    &transform,
                    light.color,
                    light.intensity});
            });
    }
}
