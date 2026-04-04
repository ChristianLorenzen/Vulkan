#include "Renderer/Scene/Extractors/CameraRenderExtractor.hpp"

namespace Faye
{
    void CameraRenderExtractor::extract(const RenderExtractionContext &context) const
    {
        const auto *primaryCamera = context.scene.getPrimaryCamera();
        context.snapshot.primaryCamera = primaryCamera != nullptr ? &primaryCamera->camera : nullptr;
    }
}