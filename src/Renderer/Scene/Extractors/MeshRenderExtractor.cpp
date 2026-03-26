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

            context.snapshot.renderables.push_back(RenderableInstance{
                renderable.entity,
                renderable.transform,
                model,
                renderable.mesh->color});
        }
    }
}