#pragma once

#include <cstdint>

#include "Scene/Camera/Camera.hpp"

namespace Faye
{
    enum class DebugViewMode
    {
        Lit,
        Depth,
        Normals,
        Wireframe
    };

    enum class RenderOutputTarget
    {
        Swapchain,
        OffscreenViewport
    };

    struct RenderView
    {
        Camera *camera = nullptr;
        uint32_t width = 0;
        uint32_t height = 0;
        RenderOutputTarget outputTarget = RenderOutputTarget::Swapchain;
        DebugViewMode debugMode = DebugViewMode::Lit;
    };
}
