#include "Editor/ImGui/Panels/RuntimeView/RuntimeViewPanel.hpp"

#include "imgui.h"

namespace Faye
{
    namespace
    {
        // The offscreen scene image is rendered with a flipped Y, so the V
        // coordinates run bottom-to-top.
        constexpr ImVec2 kViewportUvMin{0.0f, 1.0f};
        constexpr ImVec2 kViewportUvMax{1.0f, 0.0f};
    }

    void RuntimeViewPanel::draw(ImGuiFrameData &frameData,
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
            if (frameData.viewportTexture == 0 ||
                frameData.viewportSize.x <= 0.0f ||
                frameData.viewportSize.y <= 0.0f)
            {
                ImGui::TextUnformatted("Scene image unavailable.");
            }
            else
            {
                ImVec2 avail = ImGui::GetContentRegionAvail();
                float srcAspect = frameData.viewportSize.x / frameData.viewportSize.y;
                ImVec2 imageSize = avail;
                if (avail.y > 0.0f)
                {
                    float destAspect = avail.x / avail.y;
                    if (destAspect > srcAspect)
                        imageSize.x = avail.y * srcAspect;
                    else
                        imageSize.y = avail.x / srcAspect;
                }
                // Centre the letterboxed image.
                ImVec2 cursor = ImGui::GetCursorPos();
                ImGui::SetCursorPos({cursor.x + (avail.x - imageSize.x) * 0.5f,
                                     cursor.y + (avail.y - imageSize.y) * 0.5f});
                ImGui::Image(frameData.viewportTexture, imageSize, kViewportUvMin, kViewportUvMax);
            }
        }
        ImGui::End();
    }
}
