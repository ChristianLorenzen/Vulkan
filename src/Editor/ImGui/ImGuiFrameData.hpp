#pragma once

#include "Renderer/View/RenderView.hpp"
#include "imgui.h"

namespace Faye
{
    // Editor-side per-frame UI payload. Lives in the Editor layer (not the
    // renderer) so the renderer has no ImGui dependency. The editor populates
    // the viewport texture/size after recording the scene, then hands this to
    // the ImGui panels and reads back the viewport interaction state.
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
        // Reference-grid state, round-tripped like viewportDebugMode: the editor
        // seeds it from its persistent copy before drawing panels, the viewport
        // panel mutates it, and Editor::onPresent reads it back.
        EditorGridSettings viewportGrid{};
    };
}
