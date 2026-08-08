#include "Editor/Panels/EditorView/EditorViewPanel.hpp"

#include "imgui.h"
#include "imgui_internal.h"

#include <algorithm>
#include <array>
#include <utility>
#include "quill/LogMacros.h"

namespace Faye::Editor::Panels
{
    namespace
    {
        // The offscreen scene image is rendered with a flipped Y, so the V
        // coordinates run bottom-to-top.
        constexpr ImVec2 kViewportUvMin{0.0f, 1.0f};
        constexpr ImVec2 kViewportUvMax{1.0f, 0.0f};

        const char *viewportDebugModeLabel(RenderDebugMode mode)
        {
            switch (mode)
            {
            case RenderDebugMode::Lit:
                return "Lit";
            case RenderDebugMode::SceneColor:
                return "Scene Color";
            case RenderDebugMode::SceneDepth:
                return "Depth";
            case RenderDebugMode::SceneMotion:
                return "Motion";
            }

            return "Unknown";
        }

        void drawViewportDebugModeMenu(ImGuiFrameData &frameData)
        {
            if (!ImGui::BeginMenu("Viewport Output"))
                return;

            const std::array<std::pair<RenderDebugMode, const char *>, 4> modeOptions{{
                {RenderDebugMode::Lit, "Lit"},
                {RenderDebugMode::SceneColor, "Scene Color"},
                {RenderDebugMode::SceneDepth, "Depth"},
                {RenderDebugMode::SceneMotion, "Motion"},
            }};

            for (const auto &[mode, label] : modeOptions)
            {
                if (ImGui::MenuItem(label, nullptr, frameData.viewportDebugMode == mode))
                {
                    frameData.viewportDebugMode = mode;
                }
            }

            ImGui::EndMenu();
        }

        void drawViewportGridMenu(ImGuiFrameData &frameData)
        {
            EditorGridSettings &grid = frameData.viewportGrid;

            if (ImGui::MenuItem("Show Grid", "G", grid.enabled))
            {
                grid.enabled = !grid.enabled;
            }

            if (!ImGui::BeginMenu("Grid Settings"))
                return;

            // Cell size is exposed as discrete decades because the shader's LOD
            // selection already interpolates between them; an arbitrary slider
            // would just move which decade is on screen.
            const std::array<std::pair<float, const char *>, 4> cellOptions{{
                {0.1f, "10 cm"},
                {1.0f, "1 m"},
                {10.0f, "10 m"},
                {100.0f, "100 m"},
            }};

            if (ImGui::BeginMenu("Cell Size"))
            {
                for (const auto &[size, label] : cellOptions)
                {
                    if (ImGui::MenuItem(label, nullptr, grid.cellSize == size))
                        grid.cellSize = size;
                }
                ImGui::EndMenu();
            }

            ImGui::DragFloat("Fade Distance", &grid.maxDistance, 5.0f, 10.0f, 5000.0f, "%.0f m");
            ImGui::DragFloat("Plane Height", &grid.planeHeight, 0.05f, -100.0f, 100.0f, "%.2f m");
            ImGui::SliderFloat("Min Pixels/Cell", &grid.minPixelsBetweenCells, 1.0f, 8.0f, "%.1f px");
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Lower values keep fine cells visible longer, at the cost of aliasing.");

            ImGui::Separator();
            ImGui::ColorEdit4("Thin Lines", &grid.thinLineColor.x, ImGuiColorEditFlags_NoInputs);
            ImGui::ColorEdit4("Thick Lines", &grid.thickLineColor.x, ImGuiColorEditFlags_NoInputs);
            ImGui::ColorEdit4("X Axis", &grid.xAxisColor.x, ImGuiColorEditFlags_NoInputs);
            ImGui::ColorEdit4("Z Axis", &grid.zAxisColor.x, ImGuiColorEditFlags_NoInputs);

            ImGui::EndMenu();
        }

        void drawViewportContextMenu(ImGuiFrameData &frameData)
        {
            drawViewportDebugModeMenu(frameData);
            ImGui::Separator();
            drawViewportGridMenu(frameData);
            ImGui::Separator();
            ImGui::Text("Current: %s", viewportDebugModeLabel(frameData.viewportDebugMode));
        }

        std::string repeat(std::string s, int n)
        {
            // Copying given string to temporary string.
            std::string s1 = s;

            for (int i=1; i<n;i++)
                s += s1; // Concatenating strings

            return s;
        };


        void drawViewportFrameStatPanel(ImGuiFrameData &frameData) {

            ImVec2 winPos = ImGui::GetWindowPos();
            ImVec2 winSize = ImGui::GetWindowSize();

            const float padding = 10.0f;


            ImGui::SetNextWindowPos(
                { winPos.x + padding, winPos.y + padding * 4},
                ImGuiCond_Always,
                {0.0f, 0.0f});
            ImGui::SetNextWindowViewport(ImGui::GetWindowViewport()->ID);
            ImGui::SetNextWindowBgAlpha(0.4f);

            ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration
                            | ImGuiWindowFlags_NoDocking
                            | ImGuiWindowFlags_NoMove
                            | ImGuiWindowFlags_NoSavedSettings
                            | ImGuiWindowFlags_NoFocusOnAppearing
                            | ImGuiWindowFlags_NoNav
                            | ImGuiWindowFlags_AlwaysAutoResize;

            if (ImGui::Begin("##framestats", nullptr, flags))
            {
                ImGui::TextUnformatted("Frame statistics:");
                ImGui::Separator();
                ImGui::Text("Frame Time: %d ms", frameData.frameTimeMs);
                ImGui::Text("FPS: %d", frameData.averageFps);

                if (ImGui::BeginTable("##frametable", 2, ImGuiTableFlags_NoSavedSettings)) {
                    ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 150.0f);
                    ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthFixed, 70.0f);
                    for (Profiler::ResolvedScope &scope : frameData.scopes) {
                        std::string tabs = repeat("\t", scope.depth);
                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::Text("%s%s\t", tabs.c_str(), scope.name.c_str());
                        ImGui::TableSetColumnIndex(1);
                        ImGui::TextAligned(1.0f,  -FLT_MIN, "%.2f", scope.milliseconds);
                    }
                    ImGui::EndTable();
                }
                ImGui::End();
            }
        };
    }

    void EditorViewPanel::draw(ImGuiFrameData &frameData,
                               Scene *scene,
                               Entity &selectedEntity,
                               uint32_t &selectedMeshNodeIndex,
                               MaterialRegistry *materialRegistry,
                               ModelRegistry *modelRegistry,
                               const TextureThumbnailCallback *textureThumbnailCallback,
                               MaterialTemplateRegistry *materialTemplateRegistry)
    {
        (void)scene;
        (void)selectedEntity;
        (void)selectedMeshNodeIndex;
        (void)materialRegistry;
        (void)modelRegistry;
        (void)textureThumbnailCallback;
        (void)materialTemplateRegistry;

        if (!open)
            return;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        bool visible = ImGui::Begin(getName(), &open, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        ImGui::PopStyleVar();

        if (visible)
        {
            frameData.viewportHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);
            frameData.viewportFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

            ImVec2 avail = ImGui::GetContentRegionAvail();

            if (avail.x > 1.0f && avail.y > 1.0f)
            {
                // Report desired render size back to the engine for next frame's resize.
                frameData.requestedViewportSize = avail;

                if (frameData.viewportTexture != 0)
                {
                    const ImVec2 imageMin = ImGui::GetCursorScreenPos();
                    ImGui::Image(frameData.viewportTexture, avail, kViewportUvMin, kViewportUvMax);

                    drawEditorControls(frameData);
                    
                    if (this->frameStatsOpen) drawViewportFrameStatPanel(frameData);

                    if (ImGui::BeginPopupContextWindow("EditorViewContextMenu", ImGuiPopupFlags_MouseButtonRight))
                    {
                        drawViewportContextMenu(frameData);
                        ImGui::EndPopup();
                    }

                    // G toggles the grid while the viewport has focus.
                    // Guarded on focus so it does not fire while typing
                    // into an inspector field.
                    if (frameData.viewportFocused && !ImGui::GetIO().WantTextInput &&
                        ImGui::IsKeyPressed(ImGuiKey_G, false))
                    {
                        frameData.viewportGrid.enabled = !frameData.viewportGrid.enabled;
                    }

                    if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                    {
                        const ImVec2 mousePosition = ImGui::GetMousePos();
                        frameData.viewportClicked = true;
                        frameData.viewportClickUv = ImVec2(
                            std::clamp((mousePosition.x - imageMin.x) / avail.x, 0.0f, 1.0f),
                            std::clamp((mousePosition.y - imageMin.y) / avail.y, 0.0f, 1.0f));
                    }
                }
                else if (ImGui::BeginPopupContextWindow("EditorViewContextMenu", ImGuiPopupFlags_MouseButtonRight))
                {
                    drawViewportContextMenu(frameData);
                    ImGui::EndPopup();
                }
            }
        }
        ImGui::End();
    }

    void EditorViewPanel::drawEditorControls(ImGuiFrameData &frameData) {
        ImVec2 winPos = ImGui::GetWindowPos();
        ImVec2 winSize = ImGui::GetWindowSize();

        const float padding = 10.0f;

        ImGui::SetNextWindowPos(
            {winPos.x + winSize.x - padding, winPos.y + winSize.y - padding},
            ImGuiCond_Always,
            {1.0f, 1.0f});
        ImGui::SetNextWindowViewport(ImGui::GetWindowViewport()->ID);
        ImGui::SetNextWindowBgAlpha(0.0f);

        ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration
                        | ImGuiWindowFlags_NoDocking
                        | ImGuiWindowFlags_NoMove
                        | ImGuiWindowFlags_NoSavedSettings
                        | ImGuiWindowFlags_NoFocusOnAppearing
                        | ImGuiWindowFlags_NoNav;
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
        if (ImGui::Begin("##frameControls", nullptr, flags))
        {
            ImGui::PopStyleVar();

            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 15.0f);
            if (ImGui::Button("X", {40,40})) {
                LOG_INFO(Logger::get(), "Button x pressed");
                this->frameStatsOpen = !this->frameStatsOpen;
            }
            if (ImGui::Button("Y", {40, 40})) {
                LOG_INFO(Logger::get(), "Button y pressed");
            }
            ImGui::PopStyleVar();
            ImGui::End();
        }
    }
}
