#pragma once

#include "Editor/ImGui/Panels/IEditorPanel.hpp"

namespace Faye
{
    class Model;

    // Scene entity list. Entities with a mesh expand into the model's mesh-node
    // tree, so a single node can be selected and inspected on its own.
    class HierarchyPanel final : public IEditorPanel
    {
    public:
        const char *getName() const override { return "Hierarchy"; }
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

    private:
        void drawMeshNodeTree(const Entity &entity,
                              const Model &model,
                              uint32_t nodeIndex,
                              Entity &selectedEntity,
                              uint32_t &selectedMeshNodeIndex);
    };
}
