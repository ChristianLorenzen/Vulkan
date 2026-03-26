#pragma once

#include "Renderer/Scene/RenderExtractionContext.hpp"

namespace Faye
{
    class SceneRenderExtractor
    {
    public:
        virtual ~SceneRenderExtractor() = default;

        virtual void extract(const RenderExtractionContext &context) const = 0;
    };
}