#pragma once

#include <array>
#include <filesystem>
#include <functional>
#include <memory>
#include <vector>

#include "Assets/ModelRegistry.hpp"
#include "Renderer/Resources/PrimitiveType.hpp"
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
            // One entry per icon_*.png in assets/editor/icons/.
            enum class FileIcon : uint32_t
            {
                Folder, FolderOpen, Texture, Cubemap, Model, Material, Shader,
                Scene, Prefab, Script, Audio, Video, Font, Animation, Skeleton,
                Particle, Archive, Config, Data, Text, Unknown,
                Count
            };

            // Uploads the image at the given path and returns the ImGui texture
            // for it. Supplied by the editor, which owns the render backend.
            using IconTextureCallback = std::function<ImTextureID(const std::filesystem::path &)>;

            // Extension -> icon. Directories always map to Folder.
            static FileIcon iconForFile(const std::filesystem::path &path);
            // Absolute path to the icon's png under assets/editor/icons/.
            static std::filesystem::path iconTexturePath(FileIcon icon);

            const char *getName() const override { return "Asset Explorer"; }
            bool isOpen() const override { return this->open; }
            void setOpen(bool isOpen) override { this->open = isOpen; }
            void FileChangeCallback(const HotReloadEvent &event);
            void setInitialFileWatch(WatchState watchState);
            void setIconTextureCallback(IconTextureCallback callback) { iconTextureCallback = std::move(callback); }
            // Uploads every icon once and caches the texture ids by FileIcon.
            // Call again after the ImGui backend is re-initialized (swapchain
            // recreation drops the descriptors these ids point at).
            void loadIcons();
            void draw(ImGuiFrameData &frameData,
                    Scene *scene,
                    Entity &selectedEntity,
                    uint32_t &selectedMeshNodeIndex,
                    MaterialRegistry *materialRegistry,
                    ModelRegistry *modelRegistry,
                    const TextureThumbnailCallback *textureThumbnailCallback,
                    MaterialTemplateRegistry *materialTemplateRegistry) override;
        private:
            struct FileUI {
                std::string file_name;
                std::string file_path;
                FileIcon icon = FileIcon::Unknown;

                void onClick() {
                    LOG_INFO(Logger::get(), "File clicked: {}", file_name);
                }
            };

            std::filesystem::path rootPath = Paths::projects();
            std::filesystem::path currentPath = rootPath;
            std::vector<FileUI> fileViews;
            bool dirty = true;

            void revalidateCurrentDir();

            ImTextureID iconTexture(FileIcon icon) const
            {
                return iconTextures[static_cast<size_t>(icon)];
            }

            WatchState watchState;
            IconTextureCallback iconTextureCallback;
            std::array<ImTextureID, static_cast<size_t>(FileIcon::Count)> iconTextures{};
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

        MaterialRegistry *materialRegistry = nullptr;
        ModelRegistry *modelRegistry = nullptr;
        MaterialTemplateRegistry *materialTemplateRegistry = nullptr;
        TextureThumbnailCallback textureThumbnailCallback;
        HotReloadManager *hotReloadManager = nullptr;
    };
}