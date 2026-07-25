#include "Editor/ImGui/EditorPanels.hpp"

#include "Assets/ModelRegistry.hpp"
#include "Core/ECS/World.hpp"
#include "Editor/ImGui/ComponentDrawRegistry.hpp"
#include "Renderer/Material/MaterialRegistry.hpp"
#include "Renderer/Material/MaterialTemplate.hpp"
#include "Renderer/PostProcess/PostProcessEffectLibrary.hpp"
#include "Renderer/Resources/Model.hpp"
#include "Scripting/LuaScriptSystem.hpp"
#include "Scripting/ScriptComponents.hpp"
#include "Core/Logging/Logger.hpp"
#include "Core/HotReload/HotReloadSystem.hpp"
#include "Core/Path/Paths.hpp"

#include "imgui.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <filesystem>
#include <sstream>
#include <string>
#include <unordered_map>

namespace Faye
{
    namespace {
        constexpr ImVec2 kViewportUvMin{0.0f, 1.0f};
        constexpr ImVec2 kViewportUvMax{1.0f, 0.0f};

        const char *viewportDebugModeLabel(RenderDebugMode mode)
        {
            switch (mode)
            {
            case RenderDebugMode::Lit:
                return "Lit";
            case RenderDebugMode::SceneColor:
                return "Scene Color";
            case RenderDebugMode::SceneDepth:
                return "Depth";
            case RenderDebugMode::SceneMotion:
                return "Motion";
            }

            return "Unknown";
        }

        const char *textureTypeLabel(TextureType type)
        {
            switch (type)
            {
            case TextureType::Albedo:
                return "Albedo";
            case TextureType::Normal:
                return "Normal";
            case TextureType::Metallic:
                return "Metallic";
            case TextureType::Roughness:
                return "Roughness";
            case TextureType::AmbientOcclusion:
                return "Ambient Occlusion";
            case TextureType::Height:
                return "Height";
            }

            return "Unknown";
        }

        const char *materialAlphaModeLabel(MaterialAlphaMode mode)
        {
            switch (mode)
            {
            case MaterialAlphaMode::Opaque:
                return "Opaque";
            case MaterialAlphaMode::Mask:
                return "Mask";
            }

            return "Unknown";
        }

        std::string formatSubmeshList(const std::vector<size_t> &submeshIndices)
        {
            std::ostringstream builder;

            for (size_t i = 0; i < submeshIndices.size(); ++i)
            {
                if (i > 0)
                {
                    builder << ", ";
                }

                builder << submeshIndices[i];
            }

            return builder.str();
        }

        // Collect all submesh indices reachable from a mesh node (including descendants).
        void collectNodeSubmeshIndices(const std::vector<Model::MeshNode> &nodes, uint32_t nodeIndex, std::vector<uint32_t> &out)
        {
            if (nodeIndex >= nodes.size())
                return;
            const auto &node = nodes[nodeIndex];
            for (uint32_t idx : node.submeshIndices)
                out.push_back(idx);
            for (uint32_t childIdx : node.childNodeIndices)
                collectNodeSubmeshIndices(nodes, childIdx, out);
        }

        // Returns true if a node (or any of its descendants) contains renderable submeshes.
        bool nodeHasGeometry(const std::vector<Model::MeshNode> &nodes, uint32_t nodeIndex)
        {
            if (nodeIndex >= nodes.size())
                return false;
            const auto &node = nodes[nodeIndex];
            if (!node.submeshIndices.empty())
                return true;
            for (uint32_t childIdx : node.childNodeIndices)
            {
                if (nodeHasGeometry(nodes, childIdx))
                    return true;
            }
            return false;
        }

        void drawViewportDebugModeMenu(ImGuiFrameData &frameData)
        {
            if (!ImGui::BeginMenu("Viewport Output"))
                return;

            const std::array<std::pair<RenderDebugMode, const char *>, 4> modeOptions{{
                {RenderDebugMode::Lit, "Lit"},
                {RenderDebugMode::SceneColor, "Scene Color"},
                {RenderDebugMode::SceneDepth, "Depth"},
                {RenderDebugMode::SceneMotion, "Motion"},
            }};

            for (const auto &[mode, label] : modeOptions)
            {
                if (ImGui::MenuItem(label, nullptr, frameData.viewportDebugMode == mode))
                {
                    frameData.viewportDebugMode = mode;
                }
            }

            ImGui::EndMenu();
        }

        void drawViewportGridMenu(ImGuiFrameData &frameData)
        {
            EditorGridSettings &grid = frameData.viewportGrid;

            if (ImGui::MenuItem("Show Grid", "G", grid.enabled))
            {
                grid.enabled = !grid.enabled;
            }

            if (!ImGui::BeginMenu("Grid Settings"))
                return;

            // Cell size is exposed as discrete decades because the shader's LOD
            // selection already interpolates between them; an arbitrary slider
            // would just move which decade is on screen.
            const std::array<std::pair<float, const char *>, 4> cellOptions{{
                {0.1f, "10 cm"},
                {1.0f, "1 m"},
                {10.0f, "10 m"},
                {100.0f, "100 m"},
            }};

            if (ImGui::BeginMenu("Cell Size"))
            {
                for (const auto &[size, label] : cellOptions)
                {
                    if (ImGui::MenuItem(label, nullptr, grid.cellSize == size))
                        grid.cellSize = size;
                }
                ImGui::EndMenu();
            }

            ImGui::DragFloat("Fade Distance", &grid.maxDistance, 5.0f, 10.0f, 5000.0f, "%.0f m");
            ImGui::DragFloat("Plane Height", &grid.planeHeight, 0.05f, -100.0f, 100.0f, "%.2f m");
            ImGui::SliderFloat("Min Pixels/Cell", &grid.minPixelsBetweenCells, 1.0f, 8.0f, "%.1f px");
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Lower values keep fine cells visible longer, at the cost of aliasing.");

            ImGui::Separator();
            ImGui::ColorEdit4("Thin Lines", &grid.thinLineColor.x, ImGuiColorEditFlags_NoInputs);
            ImGui::ColorEdit4("Thick Lines", &grid.thickLineColor.x, ImGuiColorEditFlags_NoInputs);
            ImGui::ColorEdit4("X Axis", &grid.xAxisColor.x, ImGuiColorEditFlags_NoInputs);
            ImGui::ColorEdit4("Z Axis", &grid.zAxisColor.x, ImGuiColorEditFlags_NoInputs);

            ImGui::EndMenu();
        }

        const char *effectDisplayName(const PostProcessEffectComponent &effect)
        {
            if (const auto *definition = findPostProcessEffectDefinition(effect.definitionId))
            {
                return definition->displayName.c_str();
            }

            return effect.definitionId.c_str();
        }

        void copyNameToBuffer(std::string_view value, std::array<char, 128> &buffer)
        {
            buffer.fill('\0');

            const size_t copyLength = std::min(buffer.size() - 1, value.size());
            std::memcpy(buffer.data(), value.data(), copyLength);
        }
        
    }

    AssetExplorerPanel::FileIcon AssetExplorerPanel::iconForFile(const std::filesystem::path &path)
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

    std::filesystem::path AssetExplorerPanel::iconTexturePath(FileIcon icon)
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

    void AssetExplorerPanel::loadIcons()
    {
        if (!iconTextureCallback)
            return;

        for (size_t i = 0; i < iconTextures.size(); ++i)
        {
            iconTextures[i] = iconTextureCallback(iconTexturePath(static_cast<FileIcon>(i)));
        }
    }

    void AssetExplorerPanel::FileChangeCallback(const HotReloadEvent &event) {
        LOG_INFO(Logger::get(), "File {}: {}", static_cast<int>(event.type), event.path.string());
        if (event.type != HotReloadEventType::Modified) {
            dirty = true;
        }
    }

    void AssetExplorerPanel::setInitialFileWatch(WatchState watchState) {
        LOG_INFO(Logger::get(), "Setting initial file watch state {}", watchState.knownFiles.size());
        this->watchState = watchState;
        LOG_INFO(Logger::get(), "Root path: {}, Current path: {}, Spec: {}", this->rootPath.string(), this->currentPath.string(), watchState.spec.rootPath.string());
        this->rootPath = watchState.spec.rootPath;
        this->currentPath = this->rootPath;

        if (dirty)
        {
            revalidateCurrentDir();
        }
    }

    void AssetExplorerPanel::revalidateCurrentDir() {
        fileViews.clear();
        for (const auto &entry : std::filesystem::directory_iterator(this->currentPath))
        {
            LOG_INFO(Logger::get(), "Processing file: {}", entry.path().string());
            fileViews.push_back(FileUI{entry.path().filename().string(), entry.path().string(), iconForFile(entry.path())});
        }
        this->dirty = false;
    }

    void AssetExplorerPanel::draw(ImGuiFrameData &frameData,
                Scene *scene,
                Entity &selectedEntity,
                uint32_t &selectedMeshNodeIndex,
                MaterialRegistry *materialRegistry,
                ModelRegistry *modelRegistry,
                const TextureThumbnailCallback *textureThumbnailCallback,
                MaterialTemplateRegistry *materialTemplateRegistry)
    {
        (void)scene;
        (void)selectedEntity;
        (void)selectedMeshNodeIndex;
        (void)materialRegistry;
        (void)modelRegistry;
        (void)textureThumbnailCallback;
        (void)materialTemplateRegistry;

        if (!open)
            return;

        if (this->dirty)
        {
            revalidateCurrentDir();
        }

        if (ImGui::Begin(getName(), &open))
        {
            float width = ImGui::CalcItemWidth();
            int columnCount = width / 80; // Assuming each column is 80 pixels wide
            if (columnCount < 1) columnCount = 1;
            if (ImGui::BeginTable("File Views", columnCount)) 
            {
                // Create back icon
                ImGui::TableNextColumn();
                ImGui::ImageButton("back", iconTexture(FileIcon::Folder), ImVec2(64, 64));
                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) 
                {
                    if (this->currentPath != this->rootPath)
                    {
                        this->currentPath = this->currentPath.parent_path();
                        this->dirty = true;
                    }
                }
                ImGui::Text("%s", "Back");

                for(const auto &fileView : fileViews)
                {
                    ImGui::TableNextColumn();

                    // Draw file view
                    ImGui::PushID(fileView.file_name.c_str());
                    const ImTextureID icon = iconTexture(fileView.icon);
                    const bool clicked = icon != 0
                        ? ImGui::ImageButton("icon", icon, ImVec2(64, 64))
                        : ImGui::Button("File", ImVec2(80, 40));
                    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) 
                    {
                        //fileView.onClick();
                        LOG_INFO(Logger::get(), "File clicked: {}:{}", fileView.file_name, fileView.file_path);
                        this->currentPath.append(fileView.file_name);
                        this->dirty = true;
                    }
                    const std::string text = Paths::getFileName(fileView.file_name).c_str();
                    float text_width = ImGui::CalcTextSize(text.c_str()).x;
                    float text_offset = (100.0f - text_width) * 0.5f;
                    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + text_offset);
                    ImGui::Text("%s", text.c_str());
                    ImGui::PopID();
                }
            }
            ImGui::EndTable();
        }

        ImGui::End();
    };


    class FrameStatsPanel final : public IEditorPanel
    {
    public:
        const char *getName() const override { return "Frame Counter"; }
        bool isOpen() const override { return true; }
        void setOpen(bool open) override { alwaysOpen = open; }
        bool showInViewMenu() const override { return false; }

        void draw(ImGuiFrameData &frameData,
                    Scene *scene,
                    Entity &selectedEntity,
                    uint32_t &selectedMeshNodeIndex,
                    MaterialRegistry *materialRegistry,
                    ModelRegistry *modelRegistry,
                    const TextureThumbnailCallback *textureThumbnailCallback,
                    MaterialTemplateRegistry *materialTemplateRegistry) override
        {
            (void)scene;
            (void)selectedEntity;
            (void)selectedMeshNodeIndex;
            (void)materialRegistry;
            (void)modelRegistry;
            (void)textureThumbnailCallback;
            (void)materialTemplateRegistry;

            if (!alwaysOpen)
                return;

            ImGuiWindowFlags windowFlags =
                ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking |
                ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
                ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;

            const float padding = 10.0f;
            const ImGuiViewport *viewport = ImGui::GetMainViewport();
            ImVec2 workPos = viewport->WorkPos;
            ImVec2 workSize = viewport->WorkSize;

            ImGui::SetNextWindowPos(
                {workPos.x + workSize.x - padding, workPos.y + workSize.y - padding},
                ImGuiCond_Always,
                {1.0f, 1.0f});
            ImGui::SetNextWindowViewport(viewport->ID);
            ImGui::SetNextWindowBgAlpha(0.35f);

            if (ImGui::Begin(getName(), nullptr, windowFlags))
            {
                ImGui::TextUnformatted("Frame statistics:");
                ImGui::Separator();
                ImGui::Text("Frame Time: %d ms", frameData.frameTimeMs);
                ImGui::Text("FPS: %d", frameData.averageFps);
            }
            ImGui::End();
        }

    private:
        bool alwaysOpen = true;
    };

    class ConsolePanel final : public IEditorPanel
    {
    public:
        ConsolePanel(std::shared_ptr<ConsoleSink> consoleSink) : sink(std::move(consoleSink)) {}
        const char *getName() const override { return "Console"; }
        bool isOpen() const override { return open; }
        void setOpen(bool isOpen) override { open = isOpen; }

        void draw(ImGuiFrameData &frameData,
                    Scene *scene,
                    Entity &selectedEntity,
                    uint32_t &selectedMeshNodeIndex,
                    MaterialRegistry *materialRegistry,
                    ModelRegistry *modelRegistry,
                    const TextureThumbnailCallback *textureThumbnailCallback,
                    MaterialTemplateRegistry *materialTemplateRegistry) override
        {
            (void)frameData;
            (void)scene;
            (void)selectedEntity;
            (void)selectedMeshNodeIndex;
            (void)materialRegistry;
            (void)modelRegistry;
            (void)textureThumbnailCallback;
            (void)materialTemplateRegistry;

            if (!open)
            {
                // If the console is closed, just flush pending messages so we don't get a buildup.
                // TODO: Revisit if this is what we want. Maybe console sink should use a ring buffer.
                sink->flush_sink();
                return;
            }

            pop_messages();

            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(15.0f, 10.0f));

            if (ImGui::Begin(getName(), &open))
            {
                if (ImGui::BeginTable("##console", 3, ImGuiTableFlags_BordersInnerV))
                {
                    ImGui::TableSetupColumn("Level", ImGuiTableColumnFlags_WidthFixed, 150.0f);
                    ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthFixed, 80.0f);
                    ImGui::TableSetupColumn("Message", ImGuiTableColumnFlags_WidthStretch);

                    for (const LogMessage &log : messages)
                    {
                        ImGui::TableNextRow();
                        if ((1 << static_cast<int>(log.level)) & mask)
                            ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, logLevelColors[static_cast<int>(log.level)]);
                        ImGui::TableSetColumnIndex(0);
                        ImGui::Text("%lu", log.timestamp);
                        ImGui::TableSetColumnIndex(1);
                        ImGui::Text("%s", loglevel_to_str(log.level));
                        ImGui::TableSetColumnIndex(2);
                        ImGui::Text("%s", log.message.c_str());
                    }
                    ImGui::EndTable();
                }
            }

            if (scroll)
            {
                ImGui::SetScrollHereY(1.0f); // Auto-scroll to bottom
                scroll = false;
            }

            ImGui::End();

            ImGui::PopStyleVar();
        }

        void pop_messages()
        {
            scroll = scroll || sink->has_pending_messages();

            while (sink->has_pending_messages())
            {
                messages.push_back(sink->pop_message());
            }
        }

    private:
        std::shared_ptr<ConsoleSink> sink;
        std::vector<LogMessage> messages;
        bool open = true;
        bool scroll = false;

        // Generate log entry color mask
        const unsigned int mask = (1 << (static_cast<int>(quill::LogLevel::Critical) + 1)) - (1 << static_cast<int>(quill::LogLevel::Warning));

        const char *loglevel_to_str(quill::LogLevel level)
        {
            switch (level)
            {
            case quill::LogLevel::Debug:
                return "Debug";
            case quill::LogLevel::Info:
                return "Info";
            case quill::LogLevel::Notice:
                return "Notice";
            case quill::LogLevel::Warning:
                return "Warning";
            case quill::LogLevel::Error:
                return "Error";
            case quill::LogLevel::Critical:
                return "Critical";
            case quill::LogLevel::None:
                return "None";
            default:
                return "Unknown";
            }
        }

        const ImU32 logLevelColors[11] = {
            IM_COL32(128, 128, 128, 255), // TraceL3 - Gray
            IM_COL32(128, 128, 128, 255), // TraceL2 - Gray
            IM_COL32(128, 128, 128, 255), // TraceL1 - Gray
            IM_COL32(128, 128, 128, 255), // Debug - Gray
            IM_COL32(128, 128, 128, 255), // Info - Gray
            IM_COL32(0, 255, 255, 51),    // Notice - Cyan (20% opacity)
            IM_COL32(255, 255, 0, 51),    // Warning - Yellow (20% opacity)
            IM_COL32(255, 128, 0, 51),    // Error  - Orange (20% opacity)
            IM_COL32(255, 0, 0, 51),      // Critical - Red (20% opacity)
            IM_COL32(128, 128, 128, 255), // Trace - Gray (catch-all for all trace levels)
            IM_COL32(128, 128, 128, 255)  // None - Gray (should not be used)
        };
    };

    // Primary editor viewport — fills its panel and drives the offscreen render resolution.
    class EditorViewPanel final : public IEditorPanel
    {
    public:
        const char *getName() const override { return "Editor View"; }
        bool isOpen() const override { return open; }
        void setOpen(bool isOpen) override { open = isOpen; }

        void draw(ImGuiFrameData &frameData,
                    Scene *scene,
                    Entity &selectedEntity,
                    uint32_t &selectedMeshNodeIndex,
                    MaterialRegistry *materialRegistry,
                    ModelRegistry *modelRegistry,
                    const TextureThumbnailCallback *textureThumbnailCallback,
                    MaterialTemplateRegistry *materialTemplateRegistry) override
        {
            (void)scene;
            (void)selectedEntity;
            (void)selectedMeshNodeIndex;
            (void)materialRegistry;
            (void)modelRegistry;
            (void)textureThumbnailCallback;
            (void)materialTemplateRegistry;

            if (!open)
                return;

            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
            bool visible = ImGui::Begin(getName(), &open, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
            ImGui::PopStyleVar();

            if (visible)
            {
                frameData.viewportHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);
                frameData.viewportFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

                ImVec2 avail = ImGui::GetContentRegionAvail();
                if (avail.x > 1.0f && avail.y > 1.0f)
                {
                    // Report desired render size back to the engine for next frame's resize.
                    frameData.requestedViewportSize = avail;

                    if (frameData.viewportTexture != 0)
                    {
                        const ImVec2 imageMin = ImGui::GetCursorScreenPos();
                        ImGui::Image(frameData.viewportTexture, avail, kViewportUvMin, kViewportUvMax);

                        if (ImGui::BeginPopupContextWindow("EditorViewContextMenu", ImGuiPopupFlags_MouseButtonRight))
                        {
                            drawViewportDebugModeMenu(frameData);
                            ImGui::Separator();
                            drawViewportGridMenu(frameData);
                            ImGui::Separator();
                            ImGui::Text("Current: %s", viewportDebugModeLabel(frameData.viewportDebugMode));
                            ImGui::EndPopup();
                        }

                        // G toggles the grid while the viewport has focus.
                        // Guarded on focus so it does not fire while typing
                        // into an inspector field.
                        if (frameData.viewportFocused && !ImGui::GetIO().WantTextInput &&
                            ImGui::IsKeyPressed(ImGuiKey_G, false))
                        {
                            frameData.viewportGrid.enabled = !frameData.viewportGrid.enabled;
                        }

                        if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                        {
                            const ImVec2 mousePosition = ImGui::GetMousePos();
                            frameData.viewportClicked = true;
                            frameData.viewportClickUv = ImVec2(
                                std::clamp((mousePosition.x - imageMin.x) / avail.x, 0.0f, 1.0f),
                                std::clamp((mousePosition.y - imageMin.y) / avail.y, 0.0f, 1.0f));
                        }
                    }
                    else if (ImGui::BeginPopupContextWindow("EditorViewContextMenu", ImGuiPopupFlags_MouseButtonRight))
                    {
                        drawViewportDebugModeMenu(frameData);
                        ImGui::Separator();
                        drawViewportGridMenu(frameData);
                        ImGui::Separator();
                        ImGui::Text("Current: %s", viewportDebugModeLabel(frameData.viewportDebugMode));
                        ImGui::EndPopup();
                    }
                }
            }
            ImGui::End();
        }

    private:
        bool open = true;
    };

    // Secondary viewport showing what the runtime camera sees. Letterboxed to preserve aspect ratio.
    class RuntimeViewPanel final : public IEditorPanel
    {
    public:
        const char *getName() const override { return "Runtime View"; }
        bool isOpen() const override { return open; }
        void setOpen(bool isOpen) override { open = isOpen; }

        void draw(ImGuiFrameData &frameData,
                    Scene *scene,
                    Entity &selectedEntity,
                    uint32_t &selectedMeshNodeIndex,
                    MaterialRegistry *materialRegistry,
                    ModelRegistry *modelRegistry,
                    const TextureThumbnailCallback *textureThumbnailCallback,
                    MaterialTemplateRegistry *materialTemplateRegistry) override
        {
            (void)scene;
            (void)selectedEntity;
            (void)selectedMeshNodeIndex;
            (void)materialRegistry;
            (void)modelRegistry;
            (void)textureThumbnailCallback;
            (void)materialTemplateRegistry;

            if (!open)
                return;

            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
            bool visible = ImGui::Begin(getName(), &open, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
            ImGui::PopStyleVar();

            if (visible)
            {
                if (frameData.viewportTexture == 0 ||
                    frameData.viewportSize.x <= 0.0f ||
                    frameData.viewportSize.y <= 0.0f)
                {
                    ImGui::TextUnformatted("Scene image unavailable.");
                }
                else
                {
                    ImVec2 avail = ImGui::GetContentRegionAvail();
                    float srcAspect = frameData.viewportSize.x / frameData.viewportSize.y;
                    ImVec2 imageSize = avail;
                    if (avail.y > 0.0f)
                    {
                        float destAspect = avail.x / avail.y;
                        if (destAspect > srcAspect)
                            imageSize.x = avail.y * srcAspect;
                        else
                            imageSize.y = avail.x / srcAspect;
                    }
                    // Centre the letterboxed image.
                    ImVec2 cursor = ImGui::GetCursorPos();
                    ImGui::SetCursorPos({cursor.x + (avail.x - imageSize.x) * 0.5f,
                                            cursor.y + (avail.y - imageSize.y) * 0.5f});
                    ImGui::Image(frameData.viewportTexture, imageSize, kViewportUvMin, kViewportUvMax);
                }
            }
            ImGui::End();
        }

    private:
        bool open = false;
    };

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
                    MaterialTemplateRegistry *materialTemplateRegistry) override
        {
            (void)frameData;
            (void)materialRegistry;
            (void)textureThumbnailCallback;
            (void)materialTemplateRegistry;

            if (!open)
                return;

            if (ImGui::Begin(getName(), &open))
            {
                if (scene == nullptr)
                {
                    ImGui::TextUnformatted("No active scene.");
                }
                else
                {
                    for (Ecs::Entity entityHandle : scene->getEntities())
                    {
                        Entity entity = scene->getEntity(entityHandle);
                        const std::string_view name = entity.getName();

                        const Model *model = nullptr;
                        if (modelRegistry != nullptr)
                        {
                            if (auto *meshComp = entity.tryGet<MeshRendererComponent>())
                                model = modelRegistry->getModel(meshComp->modelHandle);
                        }

                        const bool hasNodeTree = model != nullptr &&
                                                    model->getRootNodeIndex() != Model::kInvalidNodeIndex &&
                                                    nodeHasGeometry(model->getMeshNodes(), model->getRootNodeIndex());

                        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow |
                                                    ImGuiTreeNodeFlags_SpanAvailWidth;
                        if (!hasNodeTree)
                            flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

                        const bool entitySelected = entity.handle() == selectedEntity.handle() &&
                                                    selectedMeshNodeIndex == Model::kInvalidNodeIndex;
                        if (entitySelected)
                            flags |= ImGuiTreeNodeFlags_Selected;

                        const char *displayName = name.empty() ? "<unnamed>" : name.data();
                        ImGui::PushID(static_cast<int>(entityHandle.index));

                        bool nodeOpen = ImGui::TreeNodeEx("##entity", flags, "%s", displayName);

                        if (ImGui::IsItemClicked())
                        {
                            selectedEntity = entity;
                            selectedMeshNodeIndex = Model::kInvalidNodeIndex;
                        }

                        if (nodeOpen && hasNodeTree)
                        {
                            drawMeshNodeTree(entity, *model, model->getRootNodeIndex(),
                                                selectedEntity, selectedMeshNodeIndex);
                            ImGui::TreePop();
                        }

                        ImGui::PopID();
                    }
                }
            }
            ImGui::End();
        }

    private:
        void drawMeshNodeTree(const Entity &entity,
                                const Model &model,
                                uint32_t nodeIndex,
                                Entity &selectedEntity,
                                uint32_t &selectedMeshNodeIndex)
        {
            const auto &nodes = model.getMeshNodes();
            if (nodeIndex >= nodes.size())
                return;

            const auto &node = nodes[nodeIndex];

            // Skip nodes that have no geometry anywhere in their subtree.
            if (!nodeHasGeometry(nodes, nodeIndex))
                return;

            const bool isSelected = selectedEntity.handle() == entity.handle() &&
                                    selectedMeshNodeIndex == nodeIndex;

            const bool hasChildren = [&]()
            {
                for (uint32_t childIdx : node.childNodeIndices)
                    if (nodeHasGeometry(nodes, childIdx))
                        return true;
                return false;
            }();

            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_OpenOnArrow;
            if (!hasChildren)
                flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
            if (isSelected)
                flags |= ImGuiTreeNodeFlags_Selected;

            const std::string label = node.name.empty()
                                            ? ("Mesh " + std::to_string(nodeIndex))
                                            : node.name;

            ImGui::PushID(static_cast<int>(nodeIndex));
            bool treeOpen = ImGui::TreeNodeEx("##meshnode", flags, "%s", label.c_str());

            if (ImGui::IsItemClicked())
            {
                selectedEntity = entity;
                selectedMeshNodeIndex = nodeIndex;
            }

            if (treeOpen && hasChildren)
            {
                for (uint32_t childIdx : node.childNodeIndices)
                    drawMeshNodeTree(entity, model, childIdx, selectedEntity, selectedMeshNodeIndex);
                ImGui::TreePop();
            }

            ImGui::PopID();
        }

        bool open = true;
    };

    // ---- Component drawers -------------------------------------------
    // One free function per component type, registered in the draw table
    // below. The inspector loop draws the CollapsingHeader (from the type
    // registry's name) and the remove button; drawers only draw widgets.

    void drawTransform(const ComponentDrawContext &, TransformComponent &transform)
    {
        ImGui::DragFloat3("Translation", &transform.translation.x, 0.05f);
        ImGui::DragFloat3("Rotation", &transform.rotation.x, 0.01f);
        ImGui::DragFloat3("Scale", &transform.scale.x, 0.05f, 0.01f, 100.0f);
    }

    void drawMesh(const ComponentDrawContext &, MeshRendererComponent &mesh)
    {
        ImGui::Text("Model Handle: %u", mesh.modelHandle.value);
        ImGui::Text("Material Handle: %u", mesh.materialHandle.value);
        ImGui::Checkbox("Visible", &mesh.view);
    }

    void drawCamera(const ComponentDrawContext &context, CameraComponent &camera)
    {
        ImGui::TextUnformatted(camera.primary ? "Primary Camera" : "Camera");
        if (!camera.primary && ImGui::Button("Set As Primary"))
        {
            context.entity.setPrimaryCamera();
        }
    }

    void drawPointLight(const ComponentDrawContext &, PointLightComponent &pointLight)
    {
        ImGui::ColorEdit3("Light Color", &pointLight.color.x);
        ImGui::DragFloat("Intensity", &pointLight.intensity, 0.05f, 0.0f, 100.0f);
        ImGui::DragFloat("Radius", &pointLight.radius, 0.01f, 0.01f, 10.0f);
    }

    void drawDirectionalLight(const ComponentDrawContext &, DirectionalLightComponent &light)
    {
        // Direction comes from the entity's Transform rotation — rotate the
        // entity to aim the sun.
        ImGui::ColorEdit3("Light Color", &light.color.x);
        ImGui::DragFloat("Intensity", &light.intensity, 0.05f, 0.0f, 100.0f);
        ImGui::TextDisabled("Direction follows the Transform rotation");
    }

    void drawWater(const ComponentDrawContext &, WaterComponent &water)
    {
        int divs = static_cast<int>(water.subdivisions);
        if (ImGui::DragInt("Subdivisions", &divs, 1.0f, 4, 256, "%d"))
        {
            water.subdivisions = static_cast<uint32_t>(std::clamp(divs, 4, 256));
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Changes are applied automatically by\nthe WaterSubdivision script each frame.");
    }

    void drawPostProcessStack(const ComponentDrawContext &, PostProcessStackComponent &postProcessStack)
    {
        ImGui::Checkbox("Enabled", &postProcessStack.enabled);
        ImGui::Separator();

        int moveUpIndex = -1;
        int moveDownIndex = -1;
        int removeIndex = -1;

        for (size_t i = 0; i < postProcessStack.effects.size(); ++i)
        {
            auto &effect = postProcessStack.effects[i];
            ImGui::PushID(static_cast<int>(i));

            const bool open = ImGui::TreeNodeEx(
                "Effect",
                ImGuiTreeNodeFlags_DefaultOpen,
                "%zu. %s",
                i + 1,
                effectDisplayName(effect));

            if (open)
            {
                const auto *effectDefinition = findPostProcessEffectDefinition(effect.definitionId);

                if (ImGui::BeginCombo("Type", effectDisplayName(effect)))
                {
                    for (const auto &definition : getPostProcessEffectDefinitions())
                    {
                        if (!definition.showInEditor)
                        {
                            continue;
                        }

                        const bool isSelected = definition.id == effect.definitionId;
                        if (ImGui::Selectable(definition.displayName.c_str(), isSelected))
                        {
                            const bool wasEnabled = effect.enabled;
                            effect = makeDefaultPostProcessEffect(definition.id);
                            effect.enabled = wasEnabled;
                            effectDefinition = findPostProcessEffectDefinition(effect.definitionId);
                        }

                        if (isSelected)
                        {
                            ImGui::SetItemDefaultFocus();
                        }
                    }
                    ImGui::EndCombo();
                }

                ImGui::Checkbox("Effect Enabled", &effect.enabled);

                if (effectDefinition == nullptr)
                {
                    ImGui::TextUnformatted("Effect definition is missing.");
                }
                else
                {
                    for (const auto &parameter : effectDefinition->parameters)
                    {
                        if (parameter.controlType == PostProcessParameterControlType::Color4)
                        {
                            ImGui::ColorEdit4(parameter.label.c_str(), &effect.parameters.color.x);
                            continue;
                        }

                        if (float *value = getPostProcessFloatParameter(effect, parameter.binding))
                        {
                            ImGui::SliderFloat(parameter.label.c_str(), value, parameter.minValue, parameter.maxValue);
                        }
                    }
                }

                if (ImGui::Button("Move Up") && i > 0)
                {
                    moveUpIndex = static_cast<int>(i);
                }
                ImGui::SameLine();
                if (ImGui::Button("Move Down") && i + 1 < postProcessStack.effects.size())
                {
                    moveDownIndex = static_cast<int>(i);
                }
                ImGui::SameLine();
                if (ImGui::Button("Remove"))
                {
                    removeIndex = static_cast<int>(i);
                }

                ImGui::TreePop();
            }

            ImGui::PopID();
        }

        if (removeIndex >= 0)
        {
            postProcessStack.effects.erase(postProcessStack.effects.begin() + removeIndex);
        }
        else if (moveUpIndex > 0)
        {
            std::swap(postProcessStack.effects[moveUpIndex], postProcessStack.effects[moveUpIndex - 1]);
        }
        else if (moveDownIndex >= 0 && static_cast<size_t>(moveDownIndex + 1) < postProcessStack.effects.size())
        {
            std::swap(postProcessStack.effects[moveDownIndex], postProcessStack.effects[moveDownIndex + 1]);
        }

        if (ImGui::BeginCombo("Add Effect", "Select Effect"))
        {
            for (const auto &definition : getPostProcessEffectDefinitions())
            {
                if (!definition.showInEditor)
                {
                    continue;
                }

                if (ImGui::Selectable(definition.displayName.c_str()))
                {
                    postProcessStack.effects.push_back(makeDefaultPostProcessEffect(definition.id));
                }
            }
            ImGui::EndCombo();
        }
    }

    // Scripts became real components in Phase 5: they show up here like
    // any other type, read-only (attach/detach still goes through
    // ScriptSystem/LuaScriptSystem, not the generic add/remove menu).
    void drawNativeScript(const ComponentDrawContext &, NativeScriptComponent &script)
    {
        ImGui::Text("Name: %s", script.scriptName.c_str());
        ImGui::TextWrapped("Path: %s", script.scriptPath.c_str());
    }

    void drawLuaScript(const ComponentDrawContext &, LuaScriptComponent &script)
    {
        const std::string name = std::filesystem::path(script.scriptPath).stem().string();
        ImGui::Text("Name: %s", name.c_str());
        ImGui::TextWrapped("Path: %s", script.scriptPath.c_str());
    }

    ComponentDrawRegistry makeEditorDrawRegistry()
    {
        ComponentDrawRegistry registry;
        registry.registerDrawer<TransformComponent, &drawTransform>();
        registry.registerDrawer<MeshRendererComponent, &drawMesh>();
        registry.registerDrawer<CameraComponent, &drawCamera>();
        registry.registerDrawer<PointLightComponent, &drawPointLight>();
        registry.registerDrawer<DirectionalLightComponent, &drawDirectionalLight>();
        registry.registerDrawer<WaterComponent, &drawWater>();
        registry.registerDrawer<PostProcessStackComponent, &drawPostProcessStack>();
        registry.registerDrawer<NativeScriptComponent, &drawNativeScript>();
        registry.registerDrawer<LuaScriptComponent, &drawLuaScript>();
        // RigidBody2dComponent intentionally unregistered until it has UI:
        // it shows the "(no editor for this component)" placeholder.
        return registry;
    }

    class InspectorPanel final : public IEditorPanel
    {
    public:
        const char *getName() const override { return "Inspector"; }
        bool isOpen() const override { return open; }
        void setOpen(bool isOpen) override { open = isOpen; }

        void draw(ImGuiFrameData &frameData,
                    Scene *scene,
                    Entity &selectedEntity,
                    uint32_t &selectedMeshNodeIndex,
                    MaterialRegistry *materialRegistry,
                    ModelRegistry *modelRegistry,
                    const TextureThumbnailCallback *textureThumbnailCallback,
                    MaterialTemplateRegistry *materialTemplateRegistry) override
        {
            (void)frameData;

            if (!open)
                return;

            if (ImGui::Begin(getName(), &open))
            {
                if (scene == nullptr || !selectedEntity.isValid())
                {
                    ImGui::TextUnformatted("No entity selected.");
                }
                else
                {
                    drawEntityMetadata(selectedEntity);
                    drawComponents(*scene, selectedEntity);
                    drawMaterial(selectedEntity, selectedMeshNodeIndex, materialRegistry, modelRegistry, textureThumbnailCallback, materialTemplateRegistry);
                    drawAddComponentMenu(*scene, selectedEntity);
                }
            }
            ImGui::End();
        }

    private:
        ComponentDrawRegistry drawers = makeEditorDrawRegistry();

        // The join of the two reflection tables: core's type registry
        // supplies name/has/tryGetRaw/remove per ComponentId, the editor's
        // draw registry supplies the widgets for the same id. Iterating
        // the registry means a newly registered component type shows up
        // here (and in the add menu) with zero editor edits.
        void drawComponents(Scene &scene, const Entity &entity)
        {
            Ecs::World &world = scene.getWorld();
            const Ecs::Entity handle = entity.handle();
            if (!world.alive(handle))
                return;

            const ComponentDrawContext context{entity};
            for (const Ecs::ComponentTypeInfo &info : world.types().all())
            {
                if (info.name == nullptr)              // unregistered id gap: skip
                    continue;
                void *component = info.tryGetRaw(world, handle);
                if (component == nullptr)              // entity lacks this type
                    continue;

                ImGui::PushID(int(info.id));
                const bool headerOpen =
                    ImGui::CollapsingHeader(info.name, ImGuiTreeNodeFlags_DefaultOpen);
                ImGui::SameLine(ImGui::GetContentRegionAvail().x - 20.0f);
                const bool removeRequested = ImGui::SmallButton("x");
                if (headerOpen && !removeRequested)
                    drawers.draw(info.id, context, component);
                ImGui::PopID();

                if (removeRequested)
                {
                    // Legal immediately (main thread, no view iteration
                    // running), but the swap-and-pop invalidated pool
                    // pointers — stop iterating this frame.
                    info.remove(world, handle);
                    break;
                }
            }
        }

        void drawAddComponentMenu(Scene &scene, const Entity &entity)
        {
            Ecs::World &world = scene.getWorld();
            const Ecs::Entity handle = entity.handle();
            if (!world.alive(handle))
                return;

            ImGui::Separator();
            if (ImGui::Button("Add Component"))
                ImGui::OpenPopup("AddComponent");
            if (ImGui::BeginPopup("AddComponent"))
            {
                for (const Ecs::ComponentTypeInfo &info : world.types().all())
                {
                    if (info.name == nullptr || info.has(world, handle))
                        continue;
                    if (ImGui::MenuItem(info.name))
                        info.addDefault(world, handle);
                }
                ImGui::EndPopup();
            }
        }
        void drawEntityMetadata(const Entity &entity)
        {
            std::array<char, 128> nameBuffer{};
            copyNameToBuffer(entity.getName(), nameBuffer);

            if (ImGui::InputText("Name", nameBuffer.data(), nameBuffer.size()))
            {
                entity.setName(nameBuffer.data());
            }

            ImGui::Text("Entity Index: %u", entity.handle().index);
            ImGui::Separator();
        }

        struct ModelMaterialUsage
        {
            MaterialHandle handle{};
            std::vector<size_t> submeshIndices;
        };

        void drawTexturePreview(MaterialHandle handle, const Texture &texture, const TextureThumbnailCallback *textureThumbnailCallback)
        {
            constexpr ImVec2 previewSize{72.0f, 72.0f};

            ImTextureID previewTexture = 0;
            if (textureThumbnailCallback != nullptr && *textureThumbnailCallback)
            {
                previewTexture = (*textureThumbnailCallback)(handle, texture.type);
            }

            if (previewTexture != 0)
            {
                ImGui::Image(previewTexture, previewSize);
                return;
            }

            ImGui::BeginChild("##TexturePreviewPlaceholder", previewSize, true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
            ImGui::TextUnformatted("No");
            ImGui::TextUnformatted("Preview");
            ImGui::EndChild();
        }

        void drawTextureList(MaterialHandle handle,
                                const MaterialData &materialData,
                                const TextureThumbnailCallback *textureThumbnailCallback)
        {
            if (materialData.textures.empty())
            {
                ImGui::TextUnformatted("Textures: none");
                return;
            }

            ImGui::SeparatorText("Textures");
            for (size_t i = 0; i < materialData.textures.size(); ++i)
            {
                const Texture &texture = materialData.textures[i];
                ImGui::PushID(static_cast<int>(i));

                drawTexturePreview(handle, texture, textureThumbnailCallback);
                ImGui::SameLine();

                ImGui::BeginGroup();
                ImGui::Text("%s", textureTypeLabel(texture.type));
                ImGui::TextWrapped("%s", texture.path.empty() ? "<embedded texture>" : texture.path.c_str());
                ImGui::Text("Asset ID: %u", texture.id);
                if (texture.hasPixelData())
                {
                    ImGui::Text("Size: %dx%d (%d channels)", texture.width, texture.height, texture.channels);
                }
                else
                {
                    ImGui::TextUnformatted("Pixel data unavailable.");
                }
                ImGui::EndGroup();

                ImGui::Separator();
                ImGui::PopID();
            }
        }

        void drawMaterialProperties(MaterialHandle handle,
                                    Material &material,
                                    std::string_view usageSummary,
                                    const TextureThumbnailCallback *textureThumbnailCallback,
                                    MaterialTemplateRegistry *templateRegistry)
        {
            MaterialData &materialData = material.getMaterialData();
            bool materialChanged = false;
            std::array<char, 128> nameBuffer{};
            copyNameToBuffer(materialData.name, nameBuffer);

            ImGui::Text("Material Handle: %u", handle.value);
            if (!usageSummary.empty())
            {
                ImGui::TextWrapped("Used by submeshes: %s", std::string(usageSummary).c_str());
            }

            if (ImGui::InputText("Material Name", nameBuffer.data(), nameBuffer.size()))
            {
                materialData.name = nameBuffer.data();
                materialChanged = true;
            }

            // ---- Shader template selector -----------------------------------
            if (ImGui::CollapsingHeader("Shader", ImGuiTreeNodeFlags_DefaultOpen))
            {
                // Build combo list: index 0 = built-in PBR, then registered templates.
                std::vector<std::pair<MaterialTemplateHandle, std::string>> templateItems;
                templateItems.push_back({kBuiltinPBRTemplateHandle, "Built-in PBR"});
                if (templateRegistry != nullptr)
                {
                    for (uint32_t h = 1; h <= templateRegistry->count(); ++h)
                    {
                        const MaterialTemplate *tmpl = templateRegistry->get(h);
                        if (tmpl != nullptr)
                            templateItems.push_back({h, tmpl->name});
                    }
                }

                // Find current combo index.
                int currentIndex = 0;
                for (int i = 0; i < static_cast<int>(templateItems.size()); ++i)
                {
                    if (templateItems[i].first == materialData.templateHandle)
                    {
                        currentIndex = i;
                        break;
                    }
                }

                const char *previewLabel = templateItems[currentIndex].second.c_str();
                if (ImGui::BeginCombo("Shader Template", previewLabel))
                {
                    for (int i = 0; i < static_cast<int>(templateItems.size()); ++i)
                    {
                        bool selected = (i == currentIndex);
                        if (ImGui::Selectable(templateItems[i].second.c_str(), selected))
                        {
                            materialData.templateHandle = templateItems[i].first;
                            if (templateItems[i].first == kBuiltinPBRTemplateHandle)
                            {
                                material.setPipelineConfig({"shader.vert", "shader.frag"});
                            }
                            else if (templateRegistry != nullptr)
                            {
                                const MaterialTemplate *tmpl = templateRegistry->get(templateItems[i].first);
                                if (tmpl != nullptr)
                                    material.setPipelineConfig({tmpl->vertShaderPath, tmpl->fragShaderPath});
                            }
                            materialChanged = true;
                        }
                        if (selected)
                            ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }

                // Show compiled shader paths for reference.
                const MaterialPipelineConfig &pipelineConfig = material.getPipelineConfig();
                ImGui::TextDisabled("vert: %s", pipelineConfig.vertexShaderPath.c_str());
                ImGui::TextDisabled("frag: %s", pipelineConfig.fragmentShaderPath.c_str());
            }

            // ---- Template-specific or standard properties -------------------
            const MaterialTemplate *activeTemplate = (templateRegistry != nullptr)
                                                            ? templateRegistry->get(materialData.templateHandle)
                                                            : nullptr;

            if (activeTemplate != nullptr && !activeTemplate->properties.empty())
            {
                // Custom template: show only the properties declared in the template.
                if (ImGui::CollapsingHeader("Properties", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    for (const auto &desc : activeTemplate->properties)
                    {
                        if (desc.field == "baseColorFactor" && desc.type == ShaderMember::Type::Vec4)
                        {
                            if (ImGui::ColorEdit4(desc.label.c_str(), &materialData.baseColorFactor.x))
                            {
                                materialData.opacity = materialData.baseColorFactor.a;
                                materialChanged = true;
                            }
                        }
                        else if (desc.field == "metallicFactor" && desc.type == ShaderMember::Type::Float)
                        {
                            materialChanged |= ImGui::DragFloat(desc.label.c_str(), &materialData.metallicFactor,
                                                                0.01f, desc.minVal, desc.maxVal);
                        }
                        else if (desc.field == "roughnessFactor" && desc.type == ShaderMember::Type::Float)
                        {
                            materialChanged |= ImGui::DragFloat(desc.label.c_str(), &materialData.roughnessFactor,
                                                                0.01f, desc.minVal, desc.maxVal);
                        }
                        else if (desc.field == "emissive" && desc.type == ShaderMember::Type::Vec3)
                        {
                            materialChanged |= ImGui::ColorEdit3(desc.label.c_str(), &materialData.emissive.x);
                        }
                        else if (desc.field == "emissiveIntensity" && desc.type == ShaderMember::Type::Float)
                        {
                            materialChanged |= ImGui::DragFloat(desc.label.c_str(), &materialData.emissiveIntensity,
                                                                0.1f, desc.minVal, desc.maxVal);
                        }
                        else if (desc.field == "shininess" && desc.type == ShaderMember::Type::Float)
                        {
                            materialChanged |= ImGui::DragFloat(desc.label.c_str(), &materialData.shininess,
                                                                1.0f, desc.minVal, desc.maxVal);
                        }
                    }
                }
            }
            else
            {
                // Built-in PBR: show full standard property set.
                if (ImGui::CollapsingHeader("Properties", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    materialChanged |= ImGui::ColorEdit3("Color", &materialData.color.x);

                    if (ImGui::ColorEdit4("Base Color", &materialData.baseColorFactor.x))
                    {
                        materialData.opacity = materialData.baseColorFactor.a;
                        materialChanged = true;
                    }

                    materialChanged |= ImGui::DragFloat("Metallic", &materialData.metallicFactor, 0.01f, 0.0f, 1.0f);
                    materialChanged |= ImGui::DragFloat("Roughness", &materialData.roughnessFactor, 0.01f, 0.0f, 1.0f);
                    materialChanged |= ImGui::DragFloat("Normal Scale", &materialData.normalScale, 0.01f, 0.0f, 8.0f);
                    materialChanged |= ImGui::DragFloat("Occlusion Strength", &materialData.occlusionStrength, 0.01f, 0.0f, 1.0f);

                    if (ImGui::DragFloat("Opacity", &materialData.opacity, 0.01f, 0.0f, 1.0f))
                    {
                        materialData.baseColorFactor.a = materialData.opacity;
                        materialChanged = true;
                    }

                    int alphaModeIndex = static_cast<int>(materialData.alphaMode);
                    const char *alphaModeLabels[] = {
                        materialAlphaModeLabel(MaterialAlphaMode::Opaque),
                        materialAlphaModeLabel(MaterialAlphaMode::Mask)};
                    if (ImGui::Combo("Alpha Mode", &alphaModeIndex, alphaModeLabels, IM_ARRAYSIZE(alphaModeLabels)))
                    {
                        materialData.alphaMode = static_cast<MaterialAlphaMode>(alphaModeIndex);
                        materialChanged = true;
                    }

                    if (materialData.alphaMode == MaterialAlphaMode::Mask)
                    {
                        materialChanged |= ImGui::DragFloat("Alpha Cutoff", &materialData.alphaCutoff, 0.01f, 0.0f, 1.0f);
                    }

                    materialChanged |= ImGui::DragFloat("Shininess", &materialData.shininess, 0.1f, 0.0f, 256.0f);
                    materialChanged |= ImGui::ColorEdit3("Emissive Color", &materialData.emissive.x);
                    materialChanged |= ImGui::DragFloat("Emissive Intensity", &materialData.emissiveIntensity, 0.01f, 0.0f, 10.0f);
                    materialChanged |= ImGui::Checkbox("Double Sided", &materialData.doubleSided);
                }
            }

            if (materialChanged)
            {
                material.markDirty();
            }

            drawTextureList(handle, materialData, textureThumbnailCallback);
        }

        void drawMaterialEntry(const char *label,
                                MaterialHandle handle,
                                MaterialRegistry *materialRegistry,
                                std::string_view usageSummary,
                                const TextureThumbnailCallback *textureThumbnailCallback,
                                MaterialTemplateRegistry *templateRegistry)
        {
            ImGui::PushID(static_cast<int>(handle.value));

            Material *material = materialRegistry->getMaterial(handle);
            const char *materialName = material != nullptr && !material->getName().empty()
                                            ? material->getName().c_str()
                                            : "Unnamed Material";

            if (ImGui::TreeNodeEx("##MaterialEntry", ImGuiTreeNodeFlags_DefaultOpen, "%s: %s", label, materialName))
            {
                if (material == nullptr)
                {
                    ImGui::Text("Material Handle: %u", handle.value);
                    ImGui::TextUnformatted("Material data is missing from the registry.");
                }
                else
                {
                    drawMaterialProperties(handle, *material, usageSummary, textureThumbnailCallback, templateRegistry);
                }

                ImGui::TreePop();
            }

            ImGui::PopID();
        }

        void drawMaterial(const Entity &entity,
                            uint32_t selectedMeshNodeIndex,
                            MaterialRegistry *materialRegistry,
                            ModelRegistry *modelRegistry,
                            const TextureThumbnailCallback *textureThumbnailCallback,
                            MaterialTemplateRegistry *templateRegistry)
        {
            if (materialRegistry == nullptr)
            {
                return;
            }

            auto *mesh = entity.tryGet<MeshRendererComponent>();
            if (mesh == nullptr)
            {
                return;
            }

            if (!ImGui::CollapsingHeader("Material", ImGuiTreeNodeFlags_DefaultOpen))
            {
                return;
            }

            bool drewAnyMaterial = false;

            if (mesh->materialHandle.isValid())
            {
                drawMaterialEntry(
                    "Mesh Material Override",
                    mesh->materialHandle,
                    materialRegistry,
                    {},
                    textureThumbnailCallback,
                    templateRegistry);
                drewAnyMaterial = true;
            }

            const Model *model = modelRegistry != nullptr ? modelRegistry->getModel(mesh->modelHandle) : nullptr;
            if (model != nullptr)
            {
                const auto &allSubmeshes = model->getSubmeshes();
                const auto &meshNodes = model->getMeshNodes();

                // Determine which submesh indices to show based on selection.
                std::vector<uint32_t> activeSubmeshIndices;
                if (selectedMeshNodeIndex != Model::kInvalidNodeIndex &&
                    selectedMeshNodeIndex < meshNodes.size())
                {
                    collectNodeSubmeshIndices(meshNodes, selectedMeshNodeIndex, activeSubmeshIndices);
                    const auto &node = meshNodes[selectedMeshNodeIndex];
                    const std::string nodeLabel = node.name.empty()
                                                        ? ("Mesh Node " + std::to_string(selectedMeshNodeIndex))
                                                        : node.name;
                    ImGui::SeparatorText(nodeLabel.c_str());
                }
                else
                {
                    // No node selected — show all submeshes.
                    activeSubmeshIndices.reserve(allSubmeshes.size());
                    for (uint32_t i = 0; i < static_cast<uint32_t>(allSubmeshes.size()); ++i)
                        activeSubmeshIndices.push_back(i);

                    if (!meshNodes.empty())
                        ImGui::SeparatorText("Model Materials");
                }

                // Build per-material usage from the active submesh set.
                std::vector<ModelMaterialUsage> usages;
                std::unordered_map<uint32_t, size_t> usageIndexByHandle;
                for (uint32_t submeshIdx : activeSubmeshIndices)
                {
                    if (submeshIdx >= allSubmeshes.size())
                        continue;
                    const auto &submesh = allSubmeshes[submeshIdx];
                    if (!submesh.materialHandle.isValid())
                        continue;
                    auto [it, inserted] = usageIndexByHandle.try_emplace(
                        submesh.materialHandle.value, usages.size());
                    if (inserted)
                        usages.push_back(ModelMaterialUsage{submesh.materialHandle, {}});
                    usages[it->second].submeshIndices.push_back(submeshIdx);
                }

                if (!usages.empty())
                {
                    ImGui::Text("Unique materials: %zu", usages.size());
                    ImGui::Text("Submeshes shown: %zu", activeSubmeshIndices.size());

                    for (size_t usageIndex = 0; usageIndex < usages.size(); ++usageIndex)
                    {
                        const ModelMaterialUsage &usage = usages[usageIndex];
                        const std::string usageSummary = formatSubmeshList(usage.submeshIndices);
                        const std::string label = "Material " + std::to_string(usageIndex);
                        drawMaterialEntry(
                            label.c_str(),
                            usage.handle,
                            materialRegistry,
                            usageSummary,
                            textureThumbnailCallback,
                            templateRegistry);
                        drewAnyMaterial = true;
                    }
                }
            }

            if (!drewAnyMaterial)
            {
                ImGui::TextUnformatted("No materials are attached to this mesh or model.");
            }
        }

        bool open = true;

    };


    EditorPanels::EditorPanels()
    {
        panels.push_back(std::make_unique<ConsolePanel>(Logger::getConsoleSink()));
        panels.push_back(std::make_unique<FrameStatsPanel>());
        panels.push_back(std::make_unique<EditorViewPanel>());
        panels.push_back(std::make_unique<HierarchyPanel>());
        panels.push_back(std::make_unique<InspectorPanel>());
        panels.push_back(std::make_unique<RuntimeViewPanel>());
        panels.push_back(std::make_unique<AssetExplorerPanel>());
    }

    EditorPanels::~EditorPanels() = default;

    void EditorPanels::draw(ImGuiFrameData &frameData)
    {
        drawDockspace();

        for (const auto &panel : panels)
        {
            if (!panel->isOpen())
                continue;

            // if (typeid(*panel) == typeid(AssetExplorerPanel))
            // {
            //     // Do something specific for AssetExplorerPanel
            //     panel->draw()
            // } else {
            panel->draw(frameData, boundScene, selectedEntity, selectedMeshNodeIndex, materialRegistry, modelRegistry, &textureThumbnailCallback, materialTemplateRegistry);
            //}
        }
    }

    void EditorPanels::drawDockspace()
    {
        // Pin the dockspace host window to the full OS window so the entire
        // application surface becomes the editor UI.
        const ImGuiViewport *viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);

        ImGuiWindowFlags hostFlags =
            ImGuiWindowFlags_MenuBar |
            ImGuiWindowFlags_NoDocking |
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoBringToFrontOnFocus |
            ImGuiWindowFlags_NoNavFocus;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("##DockspaceRoot", nullptr, hostFlags);
        ImGui::PopStyleVar(3);

        ImGui::DockSpace(ImGui::GetID("MainDockSpace"), ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu("View"))
            {
                for (const auto &panel : panels)
                {
                    if (!panel->showInViewMenu())
                        continue;

                    bool open = panel->isOpen();
                    if (ImGui::MenuItem(panel->getName(), nullptr, &open))
                        panel->setOpen(open);
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Primitives"))
            {
                drawPrimitiveMenuItem(PrimitiveType::Cube);
                drawPrimitiveMenuItem(PrimitiveType::Sphere);
                drawPrimitiveMenuItem(PrimitiveType::Plane);
                drawPrimitiveMenuItem(PrimitiveType::Capsule);
                drawPrimitiveMenuItem(PrimitiveType::WaterPlane);
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }

        ImGui::End();
    }

    void EditorPanels::drawPrimitiveMenuItem(PrimitiveType primitiveType)
    {
        const std::string menuLabel = std::string("Add ") + std::string(primitiveTypeName(primitiveType));
        if (!ImGui::MenuItem(menuLabel.c_str()))
        {
            return;
        }

        if (!primitiveCreateCallback)
        {
            return;
        }

        Entity createdEntity = primitiveCreateCallback(primitiveType);
        if (createdEntity.isValid())
        {
            selectedEntity = createdEntity;
        }
    }
}