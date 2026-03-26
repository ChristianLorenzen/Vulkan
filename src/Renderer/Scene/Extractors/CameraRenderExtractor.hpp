#pragma once

#include "Renderer/Scene/SceneRenderExtractor.hpp"

namespace Faye
{
    class CameraRenderExtractor final : public SceneRenderExtractor
    {
    public:
        void extract(const RenderExtractionContext &context) const override;
    };
}