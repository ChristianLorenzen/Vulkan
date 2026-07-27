#pragma once

#include "Editor/Panels/IEditorPanel.hpp"

namespace Faye::Editor::Panels
{
    // Secondary viewport showing what the runtime camera sees. Letterboxed to
    // preserve aspect ratio. Closed by default — the editor view is the one
    // you navigate in.
    class RuntimeViewPanel final : public IEditorPanel
    {
    public:
        RuntimeViewPanel() { open = false; }

        const char *getName() const override { return "Runtime View"; }
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
