#include "Editor/Panels/FrameStats/FrameStatsPanel.hpp"

#include "imgui.h"
#include "imgui_internal.h"

namespace Faye::Editor::Panels
{
     namespace {
        std::string repeat(std::string s, int n)
        {
            // Copying given string to temporary string.
            std::string s1 = s;

            for (int i=1; i<n;i++)
                s += s1; // Concatenating strings

            return s;
        }
     }
    void FrameStatsPanel::draw(ImGuiFrameData &frameData,
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

        ImGuiWindowFlags windowFlags =
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking |
            ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;

        const float padding = 10.0f;
        const ImGuiViewport *viewport = ImGui::GetMainViewport();
        ImVec2 workPos = viewport->WorkPos;
        ImVec2 workSize = viewport->WorkSize;

        ImGui::SetNextWindowPos(
            {workPos.x + workSize.x - padding, workPos.y + workSize.y - padding},
            ImGuiCond_Always,
            {1.0f, 1.0f});
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::SetNextWindowBgAlpha(0.35f);

        if (ImGui::Begin(getName(), nullptr, windowFlags))
        {
            ImGui::TextUnformatted("Frame statistics:");
            ImGui::Separator();
            ImGui::Text("Frame Time: %d ms", frameData.frameTimeMs);
            ImGui::Text("FPS: %d", frameData.averageFps);

            if (ImGui::BeginTable("##frametable", 2)) {
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
        }
        ImGui::End();
    }
}
