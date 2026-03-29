#pragma once

#include "imgui/imgui.h"

namespace Faye
{
    struct ImGuiFrameData
    {
        int frameTimeMs = 0;
        int averageFps = 0;
        ImTextureID sceneViewportTexture = 0;
        ImVec2 sceneViewportSize{0.0f, 0.0f};
        ImVec2 requestedSceneViewportSize{0.0f, 0.0f};
        bool sceneViewportHovered = false;
        bool sceneViewportFocused = false;
        bool sceneViewportClicked = false;
        ImVec2 sceneViewportClickUv{-1.0f, -1.0f};
    };
}