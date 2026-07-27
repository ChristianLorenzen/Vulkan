#pragma once

#include "Editor/ImGui/Panels/IEditorPanel.hpp"

namespace Faye
{
    // Primary editor viewport — fills its panel and drives the offscreen render
    // resolution. Also hosts the debug-output and grid menus, and reports
    // hover/focus/click state back through ImGuiFrameData.
    class EditorViewPanel final : public IEditorPanel
    {
    public:
        const char *getName() const override { return "Editor View"; }
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
