#pragma once

#include "Editor/Panels/IEditorPanel.hpp"
#include <optional>

namespace Faye
{
    class Model;
}

namespace Faye::Editor::Panels
{
    enum ContextType {
        deleteEntity,
        duplicateEntity
    };

    struct EntityContext {
        Ecs::Entity contextEntity{};
        ContextType contextType{};
    };

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
        bool matchesFilter(const char *name) const;
        std::array<char, 64> entityFilter{};
        Ecs::Entity contextEntity{};
        std::optional<EntityContext> entityContext;
    };
}
