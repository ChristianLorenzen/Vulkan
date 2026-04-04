#pragma once

#include <cstdint>

#include "Scene/Camera/Camera.hpp"

namespace Faye
{
    enum class RenderOutputTarget
    {
        Swapchain,
        OffscreenSceneColor
    };

    enum class RenderDebugMode
    {
        Lit,
        SceneColor,
        SceneDepth,
        SceneMotion
    };

    struct RenderViewport
    {
        uint32_t width = 0;
        uint32_t height = 0;

        float aspectRatio() const
        {
            return height == 0 ? 1.f : static_cast<float>(width) / static_cast<float>(height);
        }
    };

    struct RenderView
    {
        const Camera *camera = nullptr;
        RenderViewport viewport{};
        RenderOutputTarget outputTarget = RenderOutputTarget::Swapchain;
        RenderDebugMode debugMode = RenderDebugMode::Lit;
    };
}
