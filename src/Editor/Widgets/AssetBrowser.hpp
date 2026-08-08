#pragma once

#include <array>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include <imgui.h>

#include "Core/Handles/MaterialHandle.hpp"
#include "Core/Path/Paths.hpp"
#include "Renderer/Material/Material.hpp"

namespace Faye::Editor::Widgets
{
    // Icon atlas for the editor's file views. Split out of AssetExplorerPanel
    // so the panel and the texture picker share one set of uploads: the
    // textures are GPU descriptors, and uploading them per view would leak a
    // set every time a picker opens.
    class EditorIconLibrary
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

        // Uploads the image at the given path and returns the ImGui texture for
        // it. Supplied by the editor, which owns the render backend.
        using IconTextureCallback = std::function<ImTextureID(const std::filesystem::path &)>;

        // Extension -> icon. Directories always map to Folder.
        static FileIcon iconForFile(const std::filesystem::path &path);
        // Absolute path to the icon's png under assets/editor/icons/.
        static std::filesystem::path iconTexturePath(FileIcon icon);

        void setIconTextureCallback(IconTextureCallback callback) { iconTextureCallback = std::move(callback); }
        // Uploads every icon once and caches the texture ids by FileIcon. Call
        // again after the ImGui backend is re-initialized (swapchain recreation
        // drops the descriptors these ids point at).
        void loadIcons();

        ImTextureID iconTexture(FileIcon icon) const { return iconTextures[static_cast<size_t>(icon)]; }

    private:
        IconTextureCallback iconTextureCallback;
        std::array<ImTextureID, static_cast<size_t>(FileIcon::Count)> iconTextures{};
    };

    // A grid of files for one directory, with double-click navigation. Shared
    // by the Asset Explorer panel and the inspector's texture picker.
    class FileBrowserView
    {
    public:
        struct Result
        {
            bool fileActivated = false;             // a non-directory was double-clicked
            std::filesystem::path activatedPath;
        };

        // Invoked inside BeginPopupContextItem for the hovered entry.
        using ItemContextMenuFn = std::function<void(const std::filesystem::path &)>;

        void setRoot(std::filesystem::path root);
        // Normalized extensions (".png"); empty means "show everything".
        // Directories are always listed so the tree stays navigable.
        void setExtensionFilter(std::vector<std::string> extensions);
        void markDirty() { dirty = true; }

        const std::filesystem::path &getCurrentPath() const { return currentPath; }
        const std::filesystem::path &getRootPath() const { return rootPath; }

        Result draw(const EditorIconLibrary &icons, const ItemContextMenuFn &contextMenu = {});
        // Breadcrumb row ("root / sub / dir") with clickable ancestors.
        void drawBreadcrumb();

    private:
        struct Entry
        {
            std::string name;
            std::filesystem::path path;
            bool isDirectory = false;
            EditorIconLibrary::FileIcon icon = EditorIconLibrary::FileIcon::Unknown;
        };

        void revalidate();
        bool passesFilter(const std::filesystem::path &path) const;

        std::filesystem::path rootPath = Paths::projects();
        std::filesystem::path currentPath = rootPath;
        std::vector<Entry> entries;
        std::vector<std::string> extensionFilter;
        bool dirty = true;
    };

    // Modal file picker for a material texture slot. Owned by the inspector;
    // draw() must be called from the same ImGui window that opened it, at a
    // point where no extra ID scopes are pushed.
    class TexturePickerPopup
    {
    public:
        explicit TexturePickerPopup();

        // Records which slot the next accepted file belongs to and opens the
        // modal on the following draw().
        void open(MaterialHandle material, TextureType type);

        // Returns true exactly once, on the frame a file is accepted.
        bool draw(const EditorIconLibrary &icons);

        MaterialHandle requestedMaterial() const { return material; }
        TextureType requestedType() const { return type; }
        const std::filesystem::path &acceptedPath() const { return accepted; }

    private:
        FileBrowserView browser;
        MaterialHandle material{};
        TextureType type = TextureType::Albedo;
        std::filesystem::path accepted;
        bool openRequested = false;
    };
}
