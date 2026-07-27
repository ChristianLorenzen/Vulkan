#pragma once

#include <array>
#include <filesystem>
#include <functional>
#include <memory>
#include <vector>

#include "Assets/ModelRegistry.hpp"
#include "Renderer/Resources/PrimitiveType.hpp"
#include "Editor/ImGui/AssetBrowser.hpp"
#include "Editor/ImGui/ImGuiFrameData.hpp"
#include "Renderer/Material/MaterialRegistry.hpp"
#include "Renderer/Material/MaterialTemplate.hpp"
#include "Scene/Scene.hpp"
#include "Core/HotReload/HotReloadManager.hpp"
#include "Core/Path/Paths.hpp"

#include "quill/LogMacros.h"

namespace Faye
{
    class IEditorPanel
    {
    public:
        using TextureThumbnailCallback = std::function<ImTextureID(MaterialHandle, TextureType)>;

        virtual ~IEditorPanel() = default;

        virtual const char *getName() const = 0;
        virtual bool isOpen() const = 0;
        virtual void setOpen(bool open) = 0;
        virtual bool showInViewMenu() const { return true; }
        // Panels that draw file grids (asset explorer, texture picker) opt in;
        // the rest ignore it. EditorPanels owns the one shared library.
        virtual void setIconLibrary(const EditorIconLibrary *library) { (void)library; }
        virtual void draw(ImGuiFrameData &frameData,
                          Scene *scene,
                          Entity &selectedEntity,
                          uint32_t &selectedMeshNodeIndex,
                          MaterialRegistry *materialRegistry = nullptr,
                          ModelRegistry *modelRegistry = nullptr,
                          const TextureThumbnailCallback *textureThumbnailCallback = nullptr,
                          MaterialTemplateRegistry *materialTemplateRegistry = nullptr) = 0;
        
    protected:
        bool open = true;
    };

    class AssetExplorerPanel final : public IEditorPanel
    {
        public:
            using EntityCreateCallback = std::function<Entity(const std::filesystem::path &)>;

            const char *getName() const override { return "Asset Explorer"; }
            bool isOpen() const override { return this->open; }
            void setOpen(bool isOpen) override { this->open = isOpen; }
            void FileChangeCallback(const HotReloadEvent &event);
            void setInitialFileWatch(WatchState watchState);
            void setEntityCreateCallback(EntityCreateCallback callback) { entityCreateCallback = std::move(callback); }
            // Icons are uploaded once by EditorPanels and shared with the
            // inspector's texture picker.
            void setIconLibrary(const EditorIconLibrary *library) { icons = library; }
            void draw(ImGuiFrameData &frameData,
                    Scene *scene,
                    Entity &selectedEntity,
                    uint32_t &selectedMeshNodeIndex,
                    MaterialRegistry *materialRegistry,
                    ModelRegistry *modelRegistry,
                    const TextureThumbnailCallback *textureThumbnailCallback,
                    MaterialTemplateRegistry *materialTemplateRegistry) override;
        private:
            FileBrowserView browser;
            std::filesystem::path pendingCreationPath;
            WatchState watchState;
            EntityCreateCallback entityCreateCallback;
            const EditorIconLibrary *icons = nullptr;
    };

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
        void setHotReloadManager(HotReloadManager *manager) { hotReloadManager = manager; }
        void setIconTextureCallback(EditorIconLibrary::IconTextureCallback callback)
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

        EditorIconLibrary icons;
        MaterialRegistry *materialRegistry = nullptr;
        ModelRegistry *modelRegistry = nullptr;
        MaterialTemplateRegistry *materialTemplateRegistry = nullptr;
        TextureThumbnailCallback textureThumbnailCallback;
        HotReloadManager *hotReloadManager = nullptr;
    };
}