#include "Editor/Widgets/AssetBrowser.hpp"

#include <algorithm>
#include <unordered_map>

#include "Core/Logging/Logger.hpp"

#include "quill/LogMacros.h"

namespace Faye::Editor::Widgets
{
    namespace
    {
        constexpr ImVec2 kTileSize{64.0f, 64.0f};
        constexpr float kTileStride = 80.0f;   // tile + padding, drives the column count
        constexpr float kTileLabelWidth = 100.0f;
    }

    // ---- EditorIconLibrary -------------------------------------------------

    EditorIconLibrary::FileIcon EditorIconLibrary::iconForFile(const std::filesystem::path &path)
    {
        std::error_code ec;
        if (std::filesystem::is_directory(path, ec))
            return FileIcon::Folder;

        const std::string ext = Paths::normalizeExtension(path.extension().string());

        static const std::unordered_map<std::string, FileIcon> kIconsByExtension = {
            {".png", FileIcon::Texture},   {".jpg", FileIcon::Texture},   {".jpeg", FileIcon::Texture},
            {".tga", FileIcon::Texture},   {".bmp", FileIcon::Texture},   {".psd", FileIcon::Texture},
            {".hdr", FileIcon::Cubemap},   {".exr", FileIcon::Cubemap},   {".ktx", FileIcon::Cubemap},
            {".dds", FileIcon::Cubemap},
            {".obj", FileIcon::Model},     {".fbx", FileIcon::Model},     {".gltf", FileIcon::Model},
            {".glb", FileIcon::Model},     {".dae", FileIcon::Model},     {".stl", FileIcon::Model},
            {".mtl", FileIcon::Material},  {".mat", FileIcon::Material},
            {".vert", FileIcon::Shader},   {".frag", FileIcon::Shader},   {".comp", FileIcon::Shader},
            {".geom", FileIcon::Shader},   {".glsl", FileIcon::Shader},   {".spv", FileIcon::Shader},
            {".hlsl", FileIcon::Shader},
            {".scene", FileIcon::Scene},   {".faye", FileIcon::Scene},
            {".prefab", FileIcon::Prefab},
            {".lua", FileIcon::Script},    {".cpp", FileIcon::Script},    {".hpp", FileIcon::Script},
            {".h", FileIcon::Script},      {".c", FileIcon::Script},      {".py", FileIcon::Script},
            {".wav", FileIcon::Audio},     {".mp3", FileIcon::Audio},     {".ogg", FileIcon::Audio},
            {".flac", FileIcon::Audio},
            {".mp4", FileIcon::Video},     {".mov", FileIcon::Video},     {".avi", FileIcon::Video},
            {".mkv", FileIcon::Video},     {".webm", FileIcon::Video},
            {".ttf", FileIcon::Font},      {".otf", FileIcon::Font},      {".woff", FileIcon::Font},
            {".anim", FileIcon::Animation},
            {".skel", FileIcon::Skeleton},
            {".particle", FileIcon::Particle},
            {".zip", FileIcon::Archive},   {".7z", FileIcon::Archive},    {".tar", FileIcon::Archive},
            {".gz", FileIcon::Archive},    {".rar", FileIcon::Archive},
            {".ini", FileIcon::Config},    {".toml", FileIcon::Config},   {".yaml", FileIcon::Config},
            {".yml", FileIcon::Config},    {".cfg", FileIcon::Config},    {".cmake", FileIcon::Config},
            {".json", FileIcon::Data},     {".xml", FileIcon::Data},      {".csv", FileIcon::Data},
            {".bin", FileIcon::Data},
            {".txt", FileIcon::Text},      {".md", FileIcon::Text},       {".log", FileIcon::Text},
        };

        const auto it = kIconsByExtension.find(ext);
        return it != kIconsByExtension.end() ? it->second : FileIcon::Unknown;
    }

    std::filesystem::path EditorIconLibrary::iconTexturePath(FileIcon icon)
    {
        const char *fileName = "icon_unknown.png";
        switch (icon)
        {
        case FileIcon::Folder:     fileName = "icon_folder.png";      break;
        case FileIcon::FolderOpen: fileName = "icon_folder_open.png"; break;
        case FileIcon::Texture:    fileName = "icon_texture.png";     break;
        case FileIcon::Cubemap:    fileName = "icon_cubemap.png";     break;
        case FileIcon::Model:      fileName = "icon_model.png";       break;
        case FileIcon::Material:   fileName = "icon_material.png";    break;
        case FileIcon::Shader:     fileName = "icon_shader.png";      break;
        case FileIcon::Scene:      fileName = "icon_scene.png";       break;
        case FileIcon::Prefab:     fileName = "icon_prefab.png";      break;
        case FileIcon::Script:     fileName = "icon_script.png";      break;
        case FileIcon::Audio:      fileName = "icon_audio.png";       break;
        case FileIcon::Video:      fileName = "icon_video.png";       break;
        case FileIcon::Font:       fileName = "icon_font.png";        break;
        case FileIcon::Animation:  fileName = "icon_animation.png";   break;
        case FileIcon::Skeleton:   fileName = "icon_skeleton.png";    break;
        case FileIcon::Particle:   fileName = "icon_particle.png";    break;
        case FileIcon::Archive:    fileName = "icon_archive.png";     break;
        case FileIcon::Config:     fileName = "icon_config.png";      break;
        case FileIcon::Data:       fileName = "icon_data.png";        break;
        case FileIcon::Text:       fileName = "icon_text.png";        break;
        case FileIcon::Unknown:
        case FileIcon::Count:      fileName = "icon_unknown.png";     break;
        }

        return Paths::resolve("assets/editor/icons") / fileName;
    }

    void EditorIconLibrary::loadIcons()
    {
        if (!iconTextureCallback)
            return;

        for (size_t i = 0; i < iconTextures.size(); ++i)
        {
            iconTextures[i] = iconTextureCallback(iconTexturePath(static_cast<FileIcon>(i)));
        }
    }

    // ---- FileBrowserView ---------------------------------------------------

    void FileBrowserView::setRoot(std::filesystem::path root)
    {
        rootPath = std::move(root);
        currentPath = rootPath;
        dirty = true;
    }

    void FileBrowserView::setExtensionFilter(std::vector<std::string> extensions)
    {
        extensionFilter = std::move(extensions);
        for (std::string &extension : extensionFilter)
        {
            extension = Paths::normalizeExtension(extension);
        }
        dirty = true;
    }

    bool FileBrowserView::passesFilter(const std::filesystem::path &path) const
    {
        if (extensionFilter.empty())
            return true;

        const std::string ext = Paths::normalizeExtension(path.extension().string());
        return std::find(extensionFilter.begin(), extensionFilter.end(), ext) != extensionFilter.end();
    }

    void FileBrowserView::revalidate()
    {
        entries.clear();
        dirty = false;

        std::error_code ec;
        std::filesystem::directory_iterator it(currentPath, ec);
        if (ec)
        {
            LOG_WARNING(Logger::get(), "Asset browser cannot read {}: {}", currentPath.string(), ec.message());
            return;
        }

        for (const auto &entry : it)
        {
            const bool isDirectory = entry.is_directory(ec);
            if (!isDirectory && !passesFilter(entry.path()))
                continue;

            entries.push_back(Entry{
                entry.path().filename().string(),
                entry.path(),
                isDirectory,
                EditorIconLibrary::iconForFile(entry.path())});
        }

        // Directories first, then alphabetical — a raw directory_iterator is in
        // whatever order the filesystem hands back, which shuffles between runs.
        std::sort(entries.begin(), entries.end(), [](const Entry &a, const Entry &b) {
            if (a.isDirectory != b.isDirectory)
                return a.isDirectory;
            return a.name < b.name;
        });
    }

    void FileBrowserView::drawBreadcrumb()
    {
        const std::filesystem::path relative = std::filesystem::relative(currentPath, rootPath);

        ImGui::PushID("breadcrumb");
        if (ImGui::SmallButton(rootPath.filename().string().c_str()))
        {
            currentPath = rootPath;
            dirty = true;
        }

        std::filesystem::path walked = rootPath;
        if (!relative.empty() && relative != ".")
        {
            for (const auto &part : relative)
            {
                walked /= part;
                ImGui::SameLine(0.0f, 4.0f);
                ImGui::TextDisabled("/");
                ImGui::SameLine(0.0f, 4.0f);
                if (ImGui::SmallButton(part.string().c_str()))
                {
                    currentPath = walked;
                    dirty = true;
                }
            }
        }
        ImGui::PopID();
    }

    FileBrowserView::Result FileBrowserView::draw(const EditorIconLibrary &icons, const ItemContextMenuFn &contextMenu)
    {
        Result result;

        if (dirty)
        {
            revalidate();
        }

        const float width = ImGui::GetContentRegionAvail().x;
        int columnCount = static_cast<int>(width / kTileStride);
        columnCount = std::max(columnCount, 1);

        if (!ImGui::BeginTable("##fileGrid", columnCount))
            return result;

        if (currentPath != rootPath)
        {
            ImGui::TableNextColumn();
            ImGui::PushID("##back");
            const ImTextureID folderIcon = icons.iconTexture(EditorIconLibrary::FileIcon::Folder);
            if (folderIcon != 0)
                ImGui::ImageButton("back", folderIcon, kTileSize);
            else
                ImGui::Button("..", kTileSize);

            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            {
                currentPath = currentPath.parent_path();
                dirty = true;
            }
            ImGui::TextUnformatted("Back");
            ImGui::PopID();
        }

        for (const Entry &entry : entries)
        {
            ImGui::TableNextColumn();
            ImGui::PushID(entry.name.c_str());

            const ImTextureID icon = icons.iconTexture(entry.icon);
            if (icon != 0)
                ImGui::ImageButton("icon", icon, kTileSize);
            else
                ImGui::Button(entry.isDirectory ? "Dir" : "File", kTileSize);

            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            {
                // Only directories navigate. Descending into a file used to
                // produce a path that cannot be listed, leaving the grid empty.
                if (entry.isDirectory)
                {
                    currentPath = entry.path;
                    dirty = true;
                }
                else
                {
                    result.fileActivated = true;
                    result.activatedPath = entry.path;
                }
            }

            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
            {
                ImGui::SetTooltip("%s", entry.path.string().c_str());
            }

            if (contextMenu && ImGui::BeginPopupContextItem("##itemContext"))
            {
                contextMenu(entry.path);
                ImGui::EndPopup();
            }

            const float textWidth = ImGui::CalcTextSize(entry.name.c_str()).x;
            const float textOffset = std::max((kTileLabelWidth - textWidth) * 0.5f, 0.0f);
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + textOffset);
            ImGui::TextUnformatted(entry.name.c_str());

            ImGui::PopID();
        }

        ImGui::EndTable();
        return result;
    }

    // ---- TexturePickerPopup ------------------------------------------------

    namespace
    {
        const char *const kTexturePickerPopupName = "Select Texture";
    }

    TexturePickerPopup::TexturePickerPopup()
    {
        browser.setExtensionFilter({".png", ".jpg", ".jpeg", ".tga", ".bmp", ".psd", ".hdr"});
    }

    void TexturePickerPopup::open(MaterialHandle requestedMaterial, TextureType requestedType)
    {
        material = requestedMaterial;
        type = requestedType;
        accepted.clear();
        openRequested = true;
        browser.markDirty();
    }

    bool TexturePickerPopup::draw(const EditorIconLibrary &icons)
    {
        if (openRequested)
        {
            ImGui::OpenPopup(kTexturePickerPopupName);
            openRequested = false;
        }

        ImGui::SetNextWindowSize(ImVec2(640.0f, 480.0f), ImGuiCond_Appearing);
        if (!ImGui::BeginPopupModal(kTexturePickerPopupName, nullptr, ImGuiWindowFlags_NoSavedSettings))
            return false;

        browser.drawBreadcrumb();
        ImGui::Separator();

        bool acceptedThisFrame = false;
        if (ImGui::BeginChild("##pickerGrid", ImVec2(0.0f, -ImGui::GetFrameHeightWithSpacing())))
        {
            const FileBrowserView::Result result = browser.draw(icons);
            if (result.fileActivated)
            {
                accepted = result.activatedPath;
                acceptedThisFrame = true;
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndChild();

        ImGui::TextDisabled("Double-click an image to assign it.");
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
        return acceptedThisFrame;
    }
}
