#include "Editor/Panels/FrameStats/FrameStatsPanel.hpp"

#include "imgui.h"

namespace Faye::Editor::Panels
{
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
        }
        ImGui::End();
    }
}
