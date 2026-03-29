#pragma once

#include "Renderer/Scene/SceneRenderExtractor.hpp"

namespace Faye
{
    class PointLightRenderExtractor final : public SceneRenderExtractor
    {
    public:
        void extract(const RenderExtractionContext &context) const override;
    };
}