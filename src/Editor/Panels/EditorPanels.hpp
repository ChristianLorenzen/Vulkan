#pragma once

#include <array>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "Editor/Widgets/AssetBrowser.hpp"
#include "Editor/Widgets/FilePickerDialog.hpp"
#include "Editor/ImGuiFrameData.hpp"
#include "Core/Handles/PrimitiveType.hpp"
#include "Editor/Panels/IEditorPanel.hpp"
#include "engine/Scene/Scene.hpp"
namespace Faye
{
    class MaterialRegistry;
    class MaterialTemplateRegistry;
    class ModelRegistry;
}

namespace Faye::Editor::Panels
{
    // File menu actions. The editor defers them to end-of-frame; the callback
    // just records the request. `path` is empty for New/Save-with-current-path.
    enum class FileAction
    {
        New,
        Save,
        SaveAs,
        Open,
    };

    // Owns every editor panel plus the state they share (selection, asset
    // registries, icon uploads) and draws them into the dockspace each frame.
    // The panels themselves live in Editor/Panels.
    class EditorPanels
    {
    public:
        using PrimitiveCreateCallback = std::function<Entity(PrimitiveType)>;
        using TextureThumbnailCallback = IEditorPanel::TextureThumbnailCallback;
        using FileActionCallback = std::function<void(FileAction, const std::filesystem::path &)>;

        EditorPanels();
        ~EditorPanels();

        void bindScene(Scene *scene) { boundScene = scene; }
        void setPrimitiveCreateCallback(PrimitiveCreateCallback callback) { primitiveCreateCallback = std::move(callback); }
        void setFileActionCallback(FileActionCallback callback) { fileActionCallback = std::move(callback); }
        void setSelectedEntity(Entity entity) { selectedEntity = entity; }
        Entity getSelectedEntity() const { return selectedEntity; }
        void clearSelection()
        {
            selectedEntity = Entity{};
            selectedMeshNodeIndex = ~0u;
        }
        void setMaterialRegistry(MaterialRegistry *registry) { materialRegistry = registry; }
        void setModelRegistry(ModelRegistry *registry) { modelRegistry = registry; }
        void setMaterialTemplateRegistry(MaterialTemplateRegistry *registry) { materialTemplateRegistry = registry; }
        void setTextureThumbnailCallback(TextureThumbnailCallback callback) { textureThumbnailCallback = std::move(callback); }
        void setIconTextureCallback(Widgets::EditorIconLibrary::IconTextureCallback callback)
        {
            icons.setIconTextureCallback(std::move(callback));
        }
        // Re-upload after the ImGui backend is reinitialized: swapchain
        // recreation invalidates every cached texture id.
        void loadIcons() { icons.loadIcons(); }
        void draw(ImGuiFrameData &frameData);

        template<typename T>
        T* getPanelByType()
        {
            for (const auto &panel : panels)
            {
                if (dynamic_cast<T*>(panel.get()) != nullptr)
                {
                    return dynamic_cast<T*>(panel.get());
                }
            }
            return nullptr;
        }

    private:
        void drawDockspace();
        void drawPrimitiveMenuItem(PrimitiveType primitiveType);
        void drawFileMenu();
        void drawFilePicker();

        Scene *boundScene = nullptr;
        Entity selectedEntity;
        uint32_t selectedMeshNodeIndex = ~0u;

        PrimitiveCreateCallback primitiveCreateCallback;
        FileActionCallback fileActionCallback;
        std::vector<std::unique_ptr<IEditorPanel>> panels;

        // Reusable scene file picker (Open shows .faye files; Save browses a
        // folder + file name). Any specialized picker can reuse this widget.
        Widgets::FilePickerDialog filePicker;

        Widgets::EditorIconLibrary icons;
        MaterialRegistry *materialRegistry = nullptr;
        ModelRegistry *modelRegistry = nullptr;
        MaterialTemplateRegistry *materialTemplateRegistry = nullptr;
        TextureThumbnailCallback textureThumbnailCallback;
    };
}
