#include "Renderer/Scene/Extractors/MeshRenderExtractor.hpp"

#include "Core/ECS/World.hpp"

namespace Faye
{
    Ecs::SystemAccess MeshRenderExtractor::access() const
    {
        return Ecs::SystemAccess{}
            .read<TransformComponent>()
            .read<MeshRendererComponent>()
            .read<PreviousTransformComponent>();
    }

    void MeshRenderExtractor::run(Ecs::World &world, const EngineContext &,
                                  Jobs::JobSystem &, Ecs::CommandBuffer &)
    {
        world.view<TransformComponent, MeshRendererComponent>().each(
            [&](Ecs::Entity entity, TransformComponent &transform, MeshRendererComponent &mesh)
            {
                if (!mesh.view)
                {
                    return;
                }

                Model *model = models.getModel(mesh.modelHandle);
                if (model == nullptr)
                {
                    return;
                }

                Material *material = nullptr;
                if (mesh.materialHandle.isValid())
                {
                    material = materials.getMaterial(mesh.materialHandle);
                }

                const glm::mat4 modelMatrix = transform.mat4();
                glm::mat4 priorModelMatrix = modelMatrix;

                // Read last frame's transform (read-only: tryGet never creates a
                // pool, so this stays safe inside the view iteration).
                if (const auto *history = world.tryGet<PreviousTransformComponent>(entity);
                    history != nullptr && history->lastSeenExtraction + 1 == extractionIndex)
                {
                    priorModelMatrix = history->modelMatrix;
                }

                snapshot.renderables.push_back(RenderableInstance{
                    entity,
                    modelMatrix,
                    priorModelMatrix,
                    model,
                    &materials,
                    mesh.materialHandle,
                    material});
            });
    }
}
