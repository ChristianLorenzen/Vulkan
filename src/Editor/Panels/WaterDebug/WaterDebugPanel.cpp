#include "Editor/Panels/WaterDebug/WaterDebugPanel.hpp"

#include "imgui.h"

namespace Faye::Editor::Panels
{
    void WaterDebugPanel::draw(ImGuiFrameData &frameData,
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

        if (ImGui::Begin(getName(), &open))
        {
            if (frameData.waterDebugTexture == 0)
            {
                ImGui::TextUnformatted("Compute output unavailable.");
            }
            else
            {
                ImGui::TextUnformatted("water_debug_particles.comp -> 256x256 RGBA16F");
                ImGui::TextUnformatted("blue halo = pull source   red halo = push source");
                ImGui::Separator();

                // Square image, since the source is square. The particles must
                // visibly move; a frozen image means the dispatch or the time
                // push constant is not reaching the shader.
                const float side = ImGui::GetContentRegionAvail().x;
                if (side > 0.0f)
                {
                    ImGui::Image(frameData.waterDebugTexture, {side, side});
                }
            }
        }
        ImGui::End();
    }
}
