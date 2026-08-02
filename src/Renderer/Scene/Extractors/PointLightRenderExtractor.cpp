#include "Renderer/Scene/Extractors/PointLightRenderExtractor.hpp"

#include "Core/ECS/World.hpp"

namespace Faye
{
    Ecs::SystemAccess PointLightRenderExtractor::access() const
    {
        return Ecs::SystemAccess{}
            .read<TransformComponent>()
            .read<PointLightComponent>();
    }

    void PointLightRenderExtractor::run(Ecs::World &world, const EngineContext &,
                                        Jobs::JobSystem &, Ecs::CommandBuffer &)
    {
        world.view<TransformComponent, PointLightComponent>().each(
            [&](Ecs::Entity entity, TransformComponent &transform, PointLightComponent &pointLight)
            {
                snapshot.pointLights.push_back(PointLightInstance{
                    entity,
                    &transform,
                    pointLight.color,
                    pointLight.intensity,
                    pointLight.radius});
            });
    }
}
