#pragma once

#include <filesystem>
#include <functional>

#include "Core/HotReload/HotReloadManager.hpp"
#include "Editor/Widgets/AssetBrowser.hpp"
#include "Editor/Panels/IEditorPanel.hpp"

namespace Faye::Editor::Panels
{
    // Project file browser. Watches the project directory so the listing stays
    // in sync with the filesystem, and can turn a file into a scene entity
    // through a callback the editor supplies.
    class AssetExplorerPanel final : public IEditorPanel
    {
    public:
        using EntityCreateCallback = std::function<Entity(const std::filesystem::path &)>;

        const char *getName() const override { return "Asset Explorer"; }
        bool isOpen() const override { return open; }
        void setOpen(bool isOpen) override { open = isOpen; }
        void FileChangeCallback(const HotReloadEvent &event);
        void setInitialFileWatch(WatchState watchState);
        void setEntityCreateCallback(EntityCreateCallback callback) { entityCreateCallback = std::move(callback); }
        // Icons are uploaded once by EditorPanels and shared with the
        // inspector's texture picker.
        void setIconLibrary(const Widgets::EditorIconLibrary *library) override { icons = library; }

        void draw(ImGuiFrameData &frameData,
                  Scene *scene,
                  Entity &selectedEntity,
                  uint32_t &selectedMeshNodeIndex,
                  MaterialRegistry *materialRegistry,
                  ModelRegistry *modelRegistry,
                  const TextureThumbnailCallback *textureThumbnailCallback,
                  MaterialTemplateRegistry *materialTemplateRegistry) override;

    private:
        Widgets::FileBrowserView browser;
        std::filesystem::path pendingCreationPath;
        WatchState watchState;
        EntityCreateCallback entityCreateCallback;
        const Widgets::EditorIconLibrary *icons = nullptr;
    };
}
