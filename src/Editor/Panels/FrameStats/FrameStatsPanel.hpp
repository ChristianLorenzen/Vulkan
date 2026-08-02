#pragma once

#include "Editor/Panels/IEditorPanel.hpp"

namespace Faye::Editor::Panels
{
    // Frame time / FPS readout pinned to the bottom-right of the main
    // viewport. Not dockable and not listed in the View menu — it is an
    // overlay, not a window the user arranges.
    class FrameStatsPanel final : public IEditorPanel
    {
    public:
        const char *getName() const override { return "Frame Counter"; }
        bool isOpen() const override { return true; }
        void setOpen(bool isOpen) override { open = isOpen; }
        bool showInViewMenu() const override { return false; }

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
