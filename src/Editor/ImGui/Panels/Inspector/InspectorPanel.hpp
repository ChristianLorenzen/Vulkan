#pragma once

#include <array>

#include "Editor/ImGui/AssetBrowser.hpp"
#include "Editor/ImGui/ComponentDrawRegistry.hpp"
#include "Editor/ImGui/Panels/IEditorPanel.hpp"
#include "Editor/ImGui/Panels/Inspector/ComponentDrawers.hpp"

namespace Faye
{
    // Editor for the selected entity: its name, every component it carries
    // (through the draw registry), and the materials the selected mesh node
    // uses.
    class InspectorPanel final : public IEditorPanel
    {
    public:
        const char *getName() const override { return "Inspector"; }
        bool isOpen() const override { return open; }
        void setOpen(bool isOpen) override { open = isOpen; }
        void setIconLibrary(const EditorIconLibrary *library) override { icons = library; }

        void draw(ImGuiFrameData &frameData,
                  Scene *scene,
                  Entity &selectedEntity,
                  uint32_t &selectedMeshNodeIndex,
                  MaterialRegistry *materialRegistry,
                  ModelRegistry *modelRegistry,
                  const TextureThumbnailCallback *textureThumbnailCallback,
                  MaterialTemplateRegistry *materialTemplateRegistry) override;

    private:
        void drawEntityMetadata(const Entity &entity);
        // The join of the two reflection tables: core's type registry
        // supplies name/has/tryGetRaw/remove per ComponentId, the editor's
        // draw registry supplies the widgets for the same id.
        void drawComponents(Scene &scene, const ComponentDrawContext &context);
        // Materials that came in with the model, grouped by the submeshes that
        // use them. Scoped to the mesh node selected in the hierarchy, if any.
        void drawModelMaterials(const Entity &entity,
                                uint32_t selectedMeshNodeIndex,
                                const ComponentDrawContext &context);
        void drawAddComponentMenu(Scene &scene, const Entity &entity);
        void drawTexturePicker(MaterialRegistry *materialRegistry);
        bool matchesFilter(const char *name) const;

        ComponentDrawRegistry drawers = makeEditorDrawRegistry();
        TexturePickerPopup texturePicker;
        const EditorIconLibrary *icons = nullptr;
        std::array<char, 64> addComponentFilter{};
    };
}
