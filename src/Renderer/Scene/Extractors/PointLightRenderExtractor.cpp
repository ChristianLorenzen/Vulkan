#include "Renderer/Scene/Extractors/PointLightRenderExtractor.hpp"

namespace Faye
{
    void PointLightRenderExtractor::extract(const RenderExtractionContext &context) const
    {
        auto pointLights = context.scene.getPointLightViews();
        context.snapshot.pointLights.reserve(context.snapshot.pointLights.size() + pointLights.size());

        for (const auto &pointLight : pointLights)
        {
            if (pointLight.transform == nullptr || pointLight.pointLight == nullptr)
            {
                continue;
            }

            context.snapshot.pointLights.push_back(PointLightInstance{
                pointLight.entity,
                pointLight.transform,
                pointLight.pointLight->color,
                pointLight.pointLight->intensity,
                pointLight.pointLight->radius});
        }
    }
}