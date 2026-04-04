#include "Renderer/Scene/Extractors/MeshRenderExtractor.hpp"

namespace Faye
{
    void MeshRenderExtractor::extract(const RenderExtractionContext &context) const
    {
        auto renderables = context.scene.getRenderableViews();
        context.snapshot.renderables.reserve(context.snapshot.renderables.size() + renderables.size());

        for (const auto &renderable : renderables)
        {
            if (renderable.transform == nullptr || renderable.mesh == nullptr)
            {
                continue;
            }

            Model *model = context.modelRegistry.getModel(renderable.mesh->modelHandle);
            if (model == nullptr)
            {
                continue;
            }

            Material *material = context.materialRegistry.getMaterial(renderable.mesh->materialHandle);
            if (material == nullptr)
            {
                continue;
            }

            const glm::mat4 modelMatrix = renderable.transform->mat4();
            glm::mat4 priorModelMatrix = modelMatrix;

            if (const auto historyIt = context.previousModelTransforms.find(renderable.entity);
                historyIt != context.previousModelTransforms.end() &&
                historyIt->second.lastSeenExtraction + 1 == context.extractionIndex)
            {
                priorModelMatrix = historyIt->second.modelMatrix;
            }

            context.snapshot.renderables.push_back(RenderableInstance{
                renderable.entity,
                modelMatrix,
                priorModelMatrix,
                model,
                material});
        }
    }
}