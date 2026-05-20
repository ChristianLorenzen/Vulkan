#pragma once

#include "Renderer/View/RenderView.hpp"
#include "imgui.h"

namespace Faye
{
    struct ImGuiFrameData
    {
        int frameTimeMs = 0;
        int averageFps = 0;
        ImTextureID viewportTexture = 0;
        ImVec2 viewportSize{0.0f, 0.0f};
        ImVec2 requestedViewportSize{0.0f, 0.0f};
        bool viewportHovered = false;
        bool viewportFocused = false;
        bool viewportClicked = false;
        ImVec2 viewportClickUv{-1.0f, -1.0f};
        RenderDebugMode viewportDebugMode = RenderDebugMode::Lit;
    };
}