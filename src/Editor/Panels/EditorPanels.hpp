#pragma once

#include <functional>
#include <memory>
#include <vector>

#include "Editor/Widgets/AssetBrowser.hpp"
#include "Editor/ImGuiFrameData.hpp"
#include "Editor/Panels/IEditorPanel.hpp"
#include "Renderer/Resources/PrimitiveType.hpp"
#include "Scene/Scene.hpp"

namespace Faye
{
    class MaterialRegistry;
    class MaterialTemplateRegistry;
    class ModelRegistry;
}

namespace Faye::Editor::Panels
{
    // Owns every editor panel plus the state they share (selection, asset
    // registries, icon uploads) and draws them into the dockspace each frame.
    // The panels themselves live in Editor/Panels.
    class EditorPanels
    {
    public:
        using PrimitiveCreateCallback = std::function<Entity(PrimitiveType)>;
        using TextureThumbnailCallback = IEditorPanel::TextureThumbnailCallback;

        EditorPanels();
        ~EditorPanels();

        void bindScene(Scene *scene) { boundScene = scene; }
        void setPrimitiveCreateCallback(PrimitiveCreateCallback callback) { primitiveCreateCallback = std::move(callback); }
        void setSelectedEntity(Entity entity) { selectedEntity = entity; }
        Entity getSelectedEntity() const { return selectedEntity; }
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

        Scene *boundScene = nullptr;
        Entity selectedEntity;
        uint32_t selectedMeshNodeIndex = ~0u;

        PrimitiveCreateCallback primitiveCreateCallback;
        std::vector<std::unique_ptr<IEditorPanel>> panels;

        Widgets::EditorIconLibrary icons;
        MaterialRegistry *materialRegistry = nullptr;
        ModelRegistry *modelRegistry = nullptr;
        MaterialTemplateRegistry *materialTemplateRegistry = nullptr;
        TextureThumbnailCallback textureThumbnailCallback;
    };
}
