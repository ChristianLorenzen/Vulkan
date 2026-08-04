#include "Editor/Panels/SceneSettings/SceneSettingsPanel.hpp"

#include "Editor/Widgets/EditorWidgets.hpp"
#include "imgui.h"

namespace Faye::Editor::Panels
{
    void SceneSettingsPanel::draw(ImGuiFrameData &frameData,
                               Scene *scene,
                               Entity &selectedEntity,
                               uint32_t &selectedMeshNodeIndex,
                               MaterialRegistry *materialRegistry,
                               ModelRegistry *modelRegistry,
                               const TextureThumbnailCallback *textureThumbnailCallback,
                               MaterialTemplateRegistry *materialTemplateRegistry)
    {
        (void)frameData;
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
            ImGui::Text("Scene Settings");

            // Stored in radians; shown in degrees because nobody authors rotations
            // in radians. The round-trip is lossy only below float precision.
            if (Widgets::beginProperties("##skybox")) {
                Widgets::propertyLabel("Enabled", "Toggle the skybox rendering on/off.");
                ImGui::Checkbox("##enabled", &scene->getSceneSettings().skybox.enabled);
                Widgets::propertyLabel("Rotation", "Degrees. Stored internally as radians.");
                float rotationDegrees = glm::degrees(scene->getSceneSettings().skybox.rotation);
                if (ImGui::DragFloat("##rotation", &rotationDegrees, 0.5f))
                {
                    scene->getSceneSettings().skybox.rotation = glm::radians(rotationDegrees);
                }
                Widgets::propertyLabel("Skybox Intensity", "The intensity of the skybox lighting.");
                ImGui::DragFloat("##intensity", &scene->getSceneSettings().skybox.intensity, 0.01f, 0.0f, 1.0f);
                Widgets::endProperties();
            }
        }
        ImGui::End();
    }
}
