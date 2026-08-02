#pragma once

#include "Renderer/Frame/FrameContext.hpp"
#include "Renderer/Scene/RenderScene.hpp"

namespace Faye
{
    // Packs the per-frame lighting snapshot (ambient + point + directional) into
    // the GPU-side SceneLightingUBO. This is the single seam where every light
    // type is translated into GPU layout; it deliberately owns no pipeline and
    // draws nothing (a directional light has no geometry). Point-light billboards
    // remain the job of PointLightRenderSystem.
    //
    // `lighting` is expected to arrive default-constructed; ambientColor is left
    // at its default until a dedicated environment/ambient component drives it.
    void packSceneLighting(const RenderSceneSnapshot &snapshot, SceneLightingUBO &lighting);
}
