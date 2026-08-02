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

    // Infinite ground-plane reference grid, raymarched in a fragment shader.
    //
    // This is an EDITOR-ONLY affordance: it exists to keep directional bearings
    // while flying the editor camera. `enabled` defaults to false, so any view
    // built by the runtime shell gets no grid and pays no draw call. Only
    // Editor::buildRenderView() ever turns it on.
    struct EditorGridSettings
    {
        bool enabled = false;

        // World-space size of the finest grid cell, in metres. Coarser decades
        // (10x, 100x, ...) are derived from this by the shader's LOD selection.
        float cellSize = 1.0f;

        // Screen-space density target. The shader promotes to the next decade
        // once the current one would draw cells closer together than this many
        // pixels, which is what keeps the grid from aliasing into noise at
        // grazing angles or high altitude.
        float minPixelsBetweenCells = 2.0f;

        // Radius (metres, measured on the plane from the camera) at which the
        // grid has faded fully to transparent. Also bounds the useful precision
        // of the plane intersection.
        float maxDistance = 250.0f;

        // Height of the plane along world Y. Non-zero is occasionally useful for
        // aligning the grid to a floor that is not at the origin.
        float planeHeight = 0.0f;

        // Line colours. `alpha` on each scales that layer's final opacity.
        glm::vec4 thinLineColor{0.36f, 0.36f, 0.40f, 0.75f};
        glm::vec4 thickLineColor{0.52f, 0.52f, 0.58f, 0.95f};
        glm::vec4 xAxisColor{0.85f, 0.24f, 0.28f, 1.0f}; // line along +X, at Z = 0
        glm::vec4 zAxisColor{0.24f, 0.50f, 0.92f, 1.0f}; // line along +Z, at X = 0
    };

    struct SkyboxSettings {
        bool enabled = true;
    };

    struct RenderView
    {
        const Camera *camera = nullptr;
        RenderViewport viewport{};
        RenderOutputTarget outputTarget = RenderOutputTarget::Swapchain;
        RenderDebugMode debugMode = RenderDebugMode::Lit;
        EditorGridSettings grid{};
        SkyboxSettings skybox{};
    };
}
