#pragma once

#include "Editor/Panels/IEditorPanel.hpp"

namespace Faye::Editor::Panels
{
    // Displays the storage image written by water_debug_gradient.comp each
    // frame — the Phase 1 acceptance check that the compute path (reflection-
    // driven descriptors, push constants, dispatch, barriers) works end to end.
    // Temporary: replaced by real cascade/derivative views in Phase 2.
    class SceneSettingsPanel final : public IEditorPanel
    {
    public:
        const char *getName() const override { return "Scene Settings"; }
        bool isOpen() const override { return open; }
        void setOpen(bool isOpen) override { open = isOpen; }

        void draw(ImGuiFrameData &frameData,
                  Scene *scene,
                  Entity &selectedEntity,
                  uint32_t &selectedMeshNodeIndex,
                  MaterialRegistry *materialRegistry,
                  ModelRegistry *modelRegistry,
                  const TextureThumbnailCallback *textureThumbnailCallback,
                  MaterialTemplateRegistry *materialTemplateRegistry) override;
    };
}
