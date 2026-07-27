#include "Editor/ImGui/EditorPanels.hpp"

#include "Assets/ModelRegistry.hpp"
#include "Core/ECS/World.hpp"
#include "Editor/ImGui/ComponentDrawRegistry.hpp"
#include "Editor/ImGui/EditorWidgets.hpp"
#include "Renderer/Material/MaterialRegistry.hpp"
#include "Renderer/Material/MaterialTemplate.hpp"
#include "Renderer/Material/TextureLoader.hpp"
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
#include <cctype>
#include <cfloat>
#include <cstring>
#include <filesystem>
#include <sstream>
#include <string>
#include <unordered_map>

#include <glm/trigonometric.hpp>

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

    void AssetExplorerPanel::FileChangeCallback(const HotReloadEvent &event)
    {
        // Content changes don't move files around, so only creates/deletes/
        // renames need the directory listing rebuilt.
        if (event.type != HotReloadEventType::Modified)
        {
            browser.markDirty();
        }
    }

    void AssetExplorerPanel::setInitialFileWatch(WatchState watchState)
    {
        this->watchState = watchState;
        browser.setRoot(watchState.spec.rootPath);
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
        (void)frameData;
        (void)scene;
        (void)selectedEntity;
        (void)selectedMeshNodeIndex;
        (void)materialRegistry;
        (void)modelRegistry;
        (void)textureThumbnailCallback;
        (void)materialTemplateRegistry;

        if (!open)
            return;

        if (ImGui::Begin(getName(), &open))
        {
            if (icons != nullptr)
            {
                browser.drawBreadcrumb();
                ImGui::Separator();

                // Deferred: the callback imports a model and mutates the scene,
                // which is not safe to do while the popup owns the ImGui frame.
                browser.draw(*icons, [this](const std::filesystem::path &path) {
                    if (ImGui::MenuItem("Create Entity"))
                        pendingCreationPath = path;
                });

                if (!pendingCreationPath.empty())
                {
                    if (entityCreateCallback)
                        entityCreateCallback(pendingCreationPath);
                    pendingCreationPath.clear();
                }
            }
        }

        ImGui::End();
    }


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

    // ---- Material editing ---------------------------------------------
    // Free functions rather than InspectorPanel members: the Mesh Renderer
    // drawer needs them too, and drawers are plain functions in the draw table.
    namespace
    {
        // The five slots the shaders actually sample (see the push-constant
        // block in vk_render_system.cpp and MaterialCache::refreshState). Height
        // is a valid TextureType but nothing binds it, so it is not offered.
        constexpr std::array<TextureType, 5> kEditableTextureSlots{
            TextureType::Albedo,
            TextureType::Normal,
            TextureType::Metallic,
            TextureType::Roughness,
            TextureType::AmbientOcclusion};

        constexpr ImVec2 kTextureSlotSize{48.0f, 48.0f};

        const Texture *findTexture(const MaterialData &materialData, TextureType type)
        {
            for (const Texture &texture : materialData.textures)
            {
                if (texture.type == type)
                    return &texture;
            }
            return nullptr;
        }

        // Replaces the material's texture of this type, or appends one if the
        // slot was empty. Returns false (and leaves the material untouched) if
        // the file cannot be decoded.
        bool assignTexture(Material &material, TextureType type, const std::filesystem::path &path)
        {
            Texture loaded;
            try
            {
                loaded = loadTextureFromFile(path.string(), type);
            }
            catch (const std::exception &e)
            {
                LOG_WARNING(Logger::get(), "Texture assign failed for {}: {}", path.string(), e.what());
                return false;
            }

            MaterialData &materialData = material.getMaterialData();
            for (Texture &texture : materialData.textures)
            {
                if (texture.type == type)
                {
                    texture = std::move(loaded);
                    material.markDirty();
                    return true;
                }
            }

            materialData.textures.push_back(std::move(loaded));
            material.markDirty();
            return true;
        }

        void clearTexture(Material &material, TextureType type)
        {
            MaterialData &materialData = material.getMaterialData();
            const auto removed = std::remove_if(
                materialData.textures.begin(), materialData.textures.end(),
                [type](const Texture &texture) { return texture.type == type; });

            if (removed != materialData.textures.end())
            {
                materialData.textures.erase(removed, materialData.textures.end());
                // MaterialCache re-seeds the slot with the fallback texture.
                material.markDirty();
            }
        }

        // One row per shader-sampled slot, always shown so an empty slot is as
        // visible as a filled one. The thumbnail is the button.
        void drawTextureSlots(MaterialHandle handle,
                              Material &material,
                              const ComponentDrawContext::TextureThumbnailFn *thumbnails,
                              TexturePickerPopup *picker)
        {
            const MaterialData &materialData = material.getMaterialData();

            // Each slot is a 48px thumbnail row, so five of them dominate the
            // panel. Collapsed by default; the header carries the filled count
            // so the collapsed state still says whether anything is assigned.
            size_t assignedCount = 0;
            for (const TextureType type : kEditableTextureSlots)
            {
                if (findTexture(materialData, type) != nullptr)
                    ++assignedCount;
            }

            const std::string texturesLabel =
                "Textures (" + std::to_string(assignedCount) + "/" +
                std::to_string(kEditableTextureSlots.size()) + ")###textures";

            if (!EditorUI::subSection(texturesLabel.c_str(), false))
                return;

            for (const TextureType type : kEditableTextureSlots)
            {
                const Texture *texture = findTexture(materialData, type);
                ImGui::PushID(static_cast<int>(type));

                ImTextureID preview = 0;
                if (texture != nullptr && thumbnails != nullptr && *thumbnails)
                {
                    preview = (*thumbnails)(handle, type);
                }

                const bool clicked = preview != 0
                    ? ImGui::ImageButton("##slot", preview, kTextureSlotSize)
                    : ImGui::Button(texture != nullptr ? "?" : "+", kTextureSlotSize);

                if (clicked && picker != nullptr)
                {
                    picker->open(handle, type);
                }
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                {
                    ImGui::SetTooltip("%s", texture != nullptr && !texture->path.empty()
                                                ? texture->path.c_str()
                                                : "Click to assign a texture");
                }

                ImGui::SameLine();
                ImGui::BeginGroup();
                ImGui::TextUnformatted(textureTypeLabel(type));
                if (texture == nullptr)
                {
                    ImGui::TextDisabled("Empty");
                }
                else
                {
                    const std::string fileName = texture->path.empty()
                                                     ? std::string("<embedded>")
                                                     : Paths::getFileName(texture->path);
                    ImGui::TextDisabled("%s", fileName.c_str());
                    if (ImGui::SmallButton("Clear"))
                    {
                        clearTexture(material, type);
                    }
                }
                ImGui::EndGroup();

                ImGui::PopID();
            }
        }

        void drawMaterialProperties(MaterialHandle handle,
                                    Material &material,
                                    std::string_view usageSummary,
                                    const ComponentDrawContext::TextureThumbnailFn *thumbnails,
                                    MaterialTemplateRegistry *templateRegistry,
                                    TexturePickerPopup *picker)
        {
            MaterialData &materialData = material.getMaterialData();
            bool materialChanged = false;
            std::array<char, 128> nameBuffer{};
            copyNameToBuffer(materialData.name, nameBuffer);

            const std::string handleTooltip =
                "Material handle " + std::to_string(handle.value) +
                (usageSummary.empty() ? std::string{} : ("\nSubmeshes: " + std::string(usageSummary)));

            if (EditorUI::beginProperties("##materialHeader"))
            {
                EditorUI::propertyLabel("Name", handleTooltip.c_str());
                if (ImGui::InputText("##materialName", nameBuffer.data(), nameBuffer.size()))
                {
                    materialData.name = nameBuffer.data();
                    materialChanged = true;
                }

                // Built-in PBR is index 0; registered templates follow.
                std::vector<std::pair<MaterialTemplateHandle, std::string>> templateItems;
                templateItems.push_back({kBuiltinPBRTemplateHandle, "Built-in PBR"});
                if (templateRegistry != nullptr)
                {
                    for (uint32_t h = 1; h <= templateRegistry->count(); ++h)
                    {
                        if (const MaterialTemplate *tmpl = templateRegistry->get(h))
                            templateItems.push_back({h, tmpl->name});
                    }
                }

                int currentIndex = 0;
                for (int i = 0; i < static_cast<int>(templateItems.size()); ++i)
                {
                    if (templateItems[i].first == materialData.templateHandle)
                    {
                        currentIndex = i;
                        break;
                    }
                }

                const MaterialPipelineConfig &pipelineConfig = material.getPipelineConfig();
                const std::string shaderTooltip =
                    "vert: " + pipelineConfig.vertexShaderPath + "\nfrag: " + pipelineConfig.fragmentShaderPath;

                EditorUI::propertyLabel("Shader", shaderTooltip.c_str());
                if (ImGui::BeginCombo("##shaderTemplate", templateItems[currentIndex].second.c_str()))
                {
                    for (int i = 0; i < static_cast<int>(templateItems.size()); ++i)
                    {
                        const bool selected = (i == currentIndex);
                        if (ImGui::Selectable(templateItems[i].second.c_str(), selected))
                        {
                            materialData.templateHandle = templateItems[i].first;
                            if (templateItems[i].first == kBuiltinPBRTemplateHandle)
                            {
                                material.setPipelineConfig({"shader.vert", "shader.frag"});
                            }
                            else if (templateRegistry != nullptr)
                            {
                                if (const MaterialTemplate *tmpl = templateRegistry->get(templateItems[i].first))
                                    material.setPipelineConfig({tmpl->vertShaderPath, tmpl->fragShaderPath});
                            }
                            materialChanged = true;
                        }
                        if (selected)
                            ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }

                EditorUI::endProperties();
            }

            const MaterialTemplate *activeTemplate = (templateRegistry != nullptr)
                                                         ? templateRegistry->get(materialData.templateHandle)
                                                         : nullptr;

            // Collapsed by default: the full PBR set is fourteen rows, and the
            // name/shader rows above are what you need to identify a material.
            if (EditorUI::subSection("Properties###materialProperties", false) &&
                EditorUI::beginProperties("##materialProperties"))
            {
                if (activeTemplate != nullptr && !activeTemplate->properties.empty())
                {
                    // Custom template: only the properties it declares.
                    for (const auto &desc : activeTemplate->properties)
                    {
                        EditorUI::propertyLabel(desc.label.c_str());
                        if (desc.field == "baseColorFactor" && desc.type == ShaderMember::Type::Vec4)
                        {
                            if (ImGui::ColorEdit4("##baseColorFactor", &materialData.baseColorFactor.x))
                            {
                                materialData.opacity = materialData.baseColorFactor.a;
                                materialChanged = true;
                            }
                        }
                        else if (desc.field == "metallicFactor" && desc.type == ShaderMember::Type::Float)
                        {
                            materialChanged |= ImGui::DragFloat("##metallicFactor", &materialData.metallicFactor,
                                                                0.01f, desc.minVal, desc.maxVal);
                        }
                        else if (desc.field == "roughnessFactor" && desc.type == ShaderMember::Type::Float)
                        {
                            materialChanged |= ImGui::DragFloat("##roughnessFactor", &materialData.roughnessFactor,
                                                                0.01f, desc.minVal, desc.maxVal);
                        }
                        else if (desc.field == "emissive" && desc.type == ShaderMember::Type::Vec3)
                        {
                            materialChanged |= ImGui::ColorEdit3("##emissive", &materialData.emissive.x);
                        }
                        else if (desc.field == "emissiveIntensity" && desc.type == ShaderMember::Type::Float)
                        {
                            materialChanged |= ImGui::DragFloat("##emissiveIntensity", &materialData.emissiveIntensity,
                                                                0.1f, desc.minVal, desc.maxVal);
                        }
                        else if (desc.field == "shininess" && desc.type == ShaderMember::Type::Float)
                        {
                            materialChanged |= ImGui::DragFloat("##shininess", &materialData.shininess,
                                                                1.0f, desc.minVal, desc.maxVal);
                        }
                        else
                        {
                            ImGui::TextDisabled("(unsupported property type)");
                        }
                    }
                }
                else
                {
                    EditorUI::propertyLabel("Color");
                    materialChanged |= ImGui::ColorEdit3("##color", &materialData.color.x);

                    EditorUI::propertyLabel("Base Color");
                    if (ImGui::ColorEdit4("##baseColor", &materialData.baseColorFactor.x))
                    {
                        materialData.opacity = materialData.baseColorFactor.a;
                        materialChanged = true;
                    }

                    EditorUI::propertyLabel("Metallic");
                    materialChanged |= ImGui::DragFloat("##metallic", &materialData.metallicFactor, 0.01f, 0.0f, 1.0f);

                    EditorUI::propertyLabel("Roughness");
                    materialChanged |= ImGui::DragFloat("##roughness", &materialData.roughnessFactor, 0.01f, 0.0f, 1.0f);

                    EditorUI::propertyLabel("Normal Scale");
                    materialChanged |= ImGui::DragFloat("##normalScale", &materialData.normalScale, 0.01f, 0.0f, 8.0f);

                    EditorUI::propertyLabel("Occlusion");
                    materialChanged |= ImGui::DragFloat("##occlusion", &materialData.occlusionStrength, 0.01f, 0.0f, 1.0f);

                    EditorUI::propertyLabel("Opacity");
                    if (ImGui::DragFloat("##opacity", &materialData.opacity, 0.01f, 0.0f, 1.0f))
                    {
                        materialData.baseColorFactor.a = materialData.opacity;
                        materialChanged = true;
                    }

                    EditorUI::propertyLabel("Alpha Mode");
                    int alphaModeIndex = static_cast<int>(materialData.alphaMode);
                    const char *alphaModeLabels[] = {
                        materialAlphaModeLabel(MaterialAlphaMode::Opaque),
                        materialAlphaModeLabel(MaterialAlphaMode::Mask)};
                    if (ImGui::Combo("##alphaMode", &alphaModeIndex, alphaModeLabels, IM_ARRAYSIZE(alphaModeLabels)))
                    {
                        materialData.alphaMode = static_cast<MaterialAlphaMode>(alphaModeIndex);
                        materialChanged = true;
                    }

                    if (materialData.alphaMode == MaterialAlphaMode::Mask)
                    {
                        EditorUI::propertyLabel("Alpha Cutoff");
                        materialChanged |= ImGui::DragFloat("##alphaCutoff", &materialData.alphaCutoff, 0.01f, 0.0f, 1.0f);
                    }

                    EditorUI::propertyLabel("Shininess");
                    materialChanged |= ImGui::DragFloat("##shininess", &materialData.shininess, 0.1f, 0.0f, 256.0f);

                    EditorUI::propertyLabel("Emissive");
                    materialChanged |= ImGui::ColorEdit3("##emissive", &materialData.emissive.x);

                    EditorUI::propertyLabel("Emissive Intensity");
                    materialChanged |= ImGui::DragFloat("##emissiveIntensity", &materialData.emissiveIntensity, 0.01f, 0.0f, 10.0f);

                    EditorUI::propertyLabel("Double Sided");
                    materialChanged |= ImGui::Checkbox("##doubleSided", &materialData.doubleSided);
                }

                EditorUI::endProperties();
            }

            if (materialChanged)
            {
                // Bumps the revision, which is what makes MaterialCache rebuild
                // the parameter UBO and bindless slots on the next frame.
                material.markDirty();
            }

            drawTextureSlots(handle, material, thumbnails, picker);
        }

        void drawMaterialEntry(const char *label,
                               MaterialHandle handle,
                               MaterialRegistry *materialRegistry,
                               std::string_view usageSummary,
                               const ComponentDrawContext::TextureThumbnailFn *thumbnails,
                               MaterialTemplateRegistry *templateRegistry,
                               TexturePickerPopup *picker)
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
                    ImGui::TextDisabled("Material %u is missing from the registry.", handle.value);
                }
                else
                {
                    drawMaterialProperties(handle, *material, usageSummary, thumbnails, templateRegistry, picker);
                }

                ImGui::TreePop();
            }

            ImGui::PopID();
        }
    }

    // ---- Component drawers -------------------------------------------
    // One free function per component type, registered in the draw table
    // below. The inspector loop draws the card header (from the type
    // registry's name) and the remove button; drawers only draw widgets.
    // Widget labels are "##hidden" — the visible label is the property row's.

    void drawTransform(const ComponentDrawContext &, TransformComponent &transform)
    {
        if (!EditorUI::beginProperties("##transform"))
            return;

        EditorUI::propertyLabel("Position");
        ImGui::DragFloat3("##position", &transform.translation.x, 0.05f);

        // Stored in radians; shown in degrees because nobody authors rotations
        // in radians. The round-trip is lossy only below float precision.
        EditorUI::propertyLabel("Rotation", "Degrees. Stored internally as radians.");
        glm::vec3 rotationDegrees = glm::degrees(transform.rotation);
        if (ImGui::DragFloat3("##rotation", &rotationDegrees.x, 0.5f))
        {
            transform.rotation = glm::radians(rotationDegrees);
        }

        EditorUI::propertyLabel("Scale");
        ImGui::DragFloat3("##scale", &transform.scale.x, 0.05f, 0.01f, 100.0f);

        EditorUI::endProperties();
    }

    void drawMesh(const ComponentDrawContext &context, MeshRendererComponent &mesh)
    {
        if (EditorUI::beginProperties("##mesh"))
        {
            EditorUI::propertyLabel("Visible");
            ImGui::Checkbox("##visible", &mesh.view);

            // Models have no name of their own; the root node's name is what
            // the hierarchy shows, so reuse it here.
            const Model *model = context.models != nullptr ? context.models->getModel(mesh.modelHandle) : nullptr;
            std::string modelName = "None";
            if (model != nullptr)
            {
                const auto &nodes = model->getMeshNodes();
                const uint32_t rootIndex = model->getRootNodeIndex();
                if (rootIndex < nodes.size() && !nodes[rootIndex].name.empty())
                    modelName = nodes[rootIndex].name;
                else
                    modelName = "Model " + std::to_string(mesh.modelHandle.value);
            }
            const std::string modelTooltip = "Model handle " + std::to_string(mesh.modelHandle.value);
            EditorUI::propertyText("Model", modelName.c_str(), modelTooltip.c_str());

            if (context.materials != nullptr)
            {
                EditorUI::propertyLabel(
                    "Material",
                    "Overrides the materials imported with the model.\nLeave as None to keep the imported ones.");

                Material *current = context.materials->getMaterial(mesh.materialHandle);
                const char *preview = current != nullptr && !current->getName().empty()
                                          ? current->getName().c_str()
                                          : (mesh.materialHandle.isValid() ? "Unnamed Material" : "None");

                // Reserve room for the "New" button so the combo does not run
                // under it when the panel is narrow.
                const float newButtonWidth = ImGui::CalcTextSize("New").x + ImGui::GetStyle().FramePadding.x * 2.0f;
                ImGui::SetNextItemWidth(-(newButtonWidth + ImGui::GetStyle().ItemSpacing.x));
                if (ImGui::BeginCombo("##material", preview))
                {
                    if (ImGui::Selectable("None", !mesh.materialHandle.isValid()))
                    {
                        mesh.materialHandle = {};
                    }

                    for (const MaterialHandle handle : context.materials->getAllHandles())
                    {
                        const Material *candidate = context.materials->getMaterial(handle);
                        const std::string label =
                            (candidate != nullptr && !candidate->getName().empty() ? candidate->getName()
                                                                                   : std::string("Unnamed Material")) +
                            "##" + std::to_string(handle.value);
                        if (ImGui::Selectable(label.c_str(), handle.value == mesh.materialHandle.value))
                        {
                            mesh.materialHandle = handle;
                        }
                    }
                    ImGui::EndCombo();
                }

                ImGui::SameLine();
                if (ImGui::Button("New"))
                {
                    MaterialData created{};
                    created.name = "New Material";
                    mesh.materialHandle = context.materials->registerMaterial(std::move(created));
                }
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                {
                    ImGui::SetTooltip("Register a new default PBR material and assign it");
                }
            }

            EditorUI::endProperties();
        }

        // The assigned material is edited in place, right under the row that
        // assigned it, instead of in a separate section further down the panel.
        if (context.materials != nullptr && mesh.materialHandle.isValid())
        {
            if (Material *material = context.materials->getMaterial(mesh.materialHandle))
            {
                ImGui::PushID("assignedMaterial");
                drawMaterialProperties(mesh.materialHandle, *material, {}, context.thumbnails,
                                       context.materialTemplates, context.texturePicker);
                ImGui::PopID();
            }
        }
    }

    void drawCamera(const ComponentDrawContext &context, CameraComponent &camera)
    {
        if (!EditorUI::beginProperties("##camera"))
            return;

        EditorUI::propertyLabel("Primary", "The camera the runtime renders through. Only one can be primary.");
        bool primary = camera.primary;
        if (ImGui::Checkbox("##primary", &primary) && primary)
        {
            // Clearing the flag directly would leave the scene with no camera,
            // so only promotion is offered; the scene demotes the previous one.
            context.entity.setPrimaryCamera();
        }

        EditorUI::endProperties();
    }

    void drawPointLight(const ComponentDrawContext &, PointLightComponent &pointLight)
    {
        if (!EditorUI::beginProperties("##pointLight"))
            return;

        EditorUI::propertyLabel("Color");
        ImGui::ColorEdit3("##color", &pointLight.color.x);

        EditorUI::propertyLabel("Intensity");
        ImGui::DragFloat("##intensity", &pointLight.intensity, 0.05f, 0.0f, 100.0f);

        EditorUI::propertyLabel("Radius");
        ImGui::DragFloat("##radius", &pointLight.radius, 0.01f, 0.01f, 10.0f);

        EditorUI::endProperties();
    }

    void drawDirectionalLight(const ComponentDrawContext &, DirectionalLightComponent &light)
    {
        if (!EditorUI::beginProperties("##directionalLight"))
            return;

        EditorUI::propertyLabel("Color");
        ImGui::ColorEdit3("##color", &light.color.x);

        EditorUI::propertyLabel("Intensity");
        ImGui::DragFloat("##intensity", &light.intensity, 0.05f, 0.0f, 100.0f);

        // Direction comes from the entity's Transform rotation — rotate the
        // entity to aim the sun.
        EditorUI::propertyText("Direction", "From Transform rotation",
                               "Rotate the entity's Transform to aim this light.");

        EditorUI::endProperties();
    }

    void drawWater(const ComponentDrawContext &, WaterComponent &water)
    {
        if (!EditorUI::beginProperties("##water"))
            return;

        EditorUI::propertyLabel("Subdivisions",
                                "Applied automatically by the WaterSubdivision script each frame.");
        int divs = static_cast<int>(water.subdivisions);
        if (ImGui::DragInt("##subdivisions", &divs, 1.0f, 4, 256, "%d"))
        {
            water.subdivisions = static_cast<uint32_t>(std::clamp(divs, 4, 256));
        }

        EditorUI::endProperties();
    }

    void drawPostProcessStack(const ComponentDrawContext &, PostProcessStackComponent &postProcessStack)
    {
        if (EditorUI::beginProperties("##postProcess"))
        {
            EditorUI::propertyLabel("Enabled");
            ImGui::Checkbox("##enabled", &postProcessStack.enabled);
            EditorUI::endProperties();
        }

        int moveUpIndex = -1;
        int moveDownIndex = -1;
        int removeIndex = -1;

        for (size_t i = 0; i < postProcessStack.effects.size(); ++i)
        {
            auto &effect = postProcessStack.effects[i];
            ImGui::PushID(static_cast<int>(i));

            const float lineStartX = ImGui::GetCursorPosX();
            const float availableWidth = ImGui::GetContentRegionAvail().x;

            // AllowOverlap, or the full-width tree node swallows the hover for
            // the buttons drawn on top of it and none of them ever click.
            ImGui::SetNextItemAllowOverlap();
            const bool open = ImGui::TreeNodeEx(
                "Effect",
                ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap,
                "%zu. %s", i + 1, effectDisplayName(effect));

            // Reordering and removal live on the effect's own row so they read
            // as controls for that effect rather than for the whole stack.
            const float buttonWidth = ImGui::GetFrameHeight();
            ImGui::SameLine(lineStartX + availableWidth - buttonWidth * 3.0f);
            if (ImGui::Button("^", ImVec2(buttonWidth, 0.0f)) && i > 0)
                moveUpIndex = static_cast<int>(i);
            ImGui::SameLine(0.0f, 0.0f);
            if (ImGui::Button("v", ImVec2(buttonWidth, 0.0f)) && i + 1 < postProcessStack.effects.size())
                moveDownIndex = static_cast<int>(i);
            ImGui::SameLine(0.0f, 0.0f);
            if (ImGui::Button("x", ImVec2(buttonWidth, 0.0f)))
                removeIndex = static_cast<int>(i);

            if (open)
            {
                const auto *effectDefinition = findPostProcessEffectDefinition(effect.definitionId);

                if (EditorUI::beginProperties("##effect"))
                {
                    EditorUI::propertyLabel("Type");
                    if (ImGui::BeginCombo("##type", effectDisplayName(effect)))
                    {
                        for (const auto &definition : getPostProcessEffectDefinitions())
                        {
                            if (!definition.showInEditor)
                                continue;

                            const bool isSelected = definition.id == effect.definitionId;
                            if (ImGui::Selectable(definition.displayName.c_str(), isSelected))
                            {
                                const bool wasEnabled = effect.enabled;
                                effect = makeDefaultPostProcessEffect(definition.id);
                                effect.enabled = wasEnabled;
                                effectDefinition = findPostProcessEffectDefinition(effect.definitionId);
                            }

                            if (isSelected)
                                ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }

                    EditorUI::propertyLabel("Enabled");
                    ImGui::Checkbox("##effectEnabled", &effect.enabled);

                    if (effectDefinition == nullptr)
                    {
                        EditorUI::propertyText("Definition", "missing");
                    }
                    else
                    {
                        for (const auto &parameter : effectDefinition->parameters)
                        {
                            EditorUI::propertyLabel(parameter.label.c_str());
                            if (parameter.controlType == PostProcessParameterControlType::Color4)
                            {
                                ImGui::ColorEdit4("##color", &effect.parameters.color.x);
                                continue;
                            }

                            if (float *value = getPostProcessFloatParameter(effect, parameter.binding))
                            {
                                ImGui::SliderFloat("##value", value, parameter.minValue, parameter.maxValue);
                            }
                            else
                            {
                                ImGui::TextDisabled("(unbound)");
                            }
                        }
                    }

                    EditorUI::endProperties();
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

        if (ImGui::BeginCombo("##addEffect", "Add Effect"))
        {
            for (const auto &definition : getPostProcessEffectDefinitions())
            {
                if (!definition.showInEditor)
                    continue;

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
    // ScriptSystem/LuaScriptSystem, not the generic add/remove menu). One row
    // each — the path is diagnostic, so it lives in the tooltip.
    void drawNativeScript(const ComponentDrawContext &, NativeScriptComponent &script)
    {
        if (!EditorUI::beginProperties("##nativeScript"))
            return;

        EditorUI::propertyText("Script",
                               script.scriptName.empty() ? "<unbound>" : script.scriptName.c_str(),
                               script.scriptPath.empty() ? nullptr : script.scriptPath.c_str());

        EditorUI::endProperties();
    }

    void drawLuaScript(const ComponentDrawContext &, LuaScriptComponent &script)
    {
        if (!EditorUI::beginProperties("##luaScript"))
            return;

        const std::string name = std::filesystem::path(script.scriptPath).stem().string();
        EditorUI::propertyText("Script",
                               name.empty() ? "<unbound>" : name.c_str(),
                               script.scriptPath.empty() ? nullptr : script.scriptPath.c_str());

        EditorUI::endProperties();
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
        void setIconLibrary(const EditorIconLibrary *library) override { icons = library; }

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
                    const ComponentDrawContext context{
                        selectedEntity,
                        materialRegistry,
                        modelRegistry,
                        textureThumbnailCallback,
                        materialTemplateRegistry,
                        &texturePicker};

                    drawEntityMetadata(selectedEntity);
                    drawComponents(*scene, context);
                    drawModelMaterials(selectedEntity, selectedMeshNodeIndex, context);
                    drawAddComponentMenu(*scene, selectedEntity);

                    // Drawn last and at the window's top level: a modal opened
                    // from inside a card's child window would be scoped to it.
                    drawTexturePicker(materialRegistry);
                }
            }
            ImGui::End();
        }

    private:
        ComponentDrawRegistry drawers = makeEditorDrawRegistry();
        TexturePickerPopup texturePicker;
        const EditorIconLibrary *icons = nullptr;

        // The join of the two reflection tables: core's type registry
        // supplies name/has/tryGetRaw/remove per ComponentId, the editor's
        // draw registry supplies the widgets for the same id. Iterating
        // the registry means a newly registered component type shows up
        // here (and in the add menu) with zero editor edits.
        void drawComponents(Scene &scene, const ComponentDrawContext &context)
        {
            Ecs::World &world = scene.getWorld();
            const Ecs::Entity handle = context.entity.handle();
            if (!world.alive(handle))
                return;

            // Removal is deferred: info.remove swap-and-pops the pool, which
            // invalidates every component pointer this loop is holding.
            const Ecs::ComponentTypeInfo *pendingRemoval = nullptr;

            for (const Ecs::ComponentTypeInfo &info : world.types().all())
            {
                if (info.name == nullptr)              // unregistered id gap: skip
                    continue;
                void *component = info.tryGetRaw(world, handle);
                if (component == nullptr)              // entity lacks this type
                    continue;

                // Transform is not removable: every system that positions an
                // entity assumes it, and there is no way to add it back that
                // restores the previous values.
                const bool removable = info.id != Ecs::componentId<TransformComponent>();

                ImGui::PushID(int(info.id));
                const EditorUI::ComponentCard card = EditorUI::beginComponentCard(info.name, removable);
                if (card.open)
                {
                    drawers.draw(info.id, context, component);
                }
                EditorUI::endComponentCard(card);
                ImGui::PopID();

                if (card.removeRequested)
                {
                    pendingRemoval = &info;
                }
            }

            if (pendingRemoval != nullptr)
            {
                pendingRemoval->remove(world, handle);
            }
        }

        void drawAddComponentMenu(Scene &scene, const Entity &entity)
        {
            Ecs::World &world = scene.getWorld();
            const Ecs::Entity handle = entity.handle();
            if (!world.alive(handle))
                return;

            ImGui::Separator();
            if (ImGui::Button("Add Component", ImVec2(-FLT_MIN, 0.0f)))
            {
                addComponentFilter.fill('\0');
                ImGui::OpenPopup("AddComponent");
            }

            if (ImGui::BeginPopup("AddComponent"))
            {
                ImGui::SetNextItemWidth(220.0f);
                if (ImGui::IsWindowAppearing())
                    ImGui::SetKeyboardFocusHere();
                ImGui::InputTextWithHint("##filter", "Filter", addComponentFilter.data(), addComponentFilter.size());
                ImGui::Separator();

                for (const Ecs::ComponentTypeInfo &info : world.types().all())
                {
                    if (info.name == nullptr || info.has(world, handle))
                        continue;
                    if (!matchesFilter(info.name))
                        continue;
                    if (ImGui::MenuItem(info.name))
                        info.addDefault(world, handle);
                }
                ImGui::EndPopup();
            }
        }

        bool matchesFilter(const char *name) const
        {
            if (addComponentFilter[0] == '\0')
                return true;

            // Case-insensitive substring match, so "light" finds "Point Light".
            std::string lowerName(name);
            std::string lowerFilter(addComponentFilter.data());
            const auto toLower = [](std::string &value) {
                std::transform(value.begin(), value.end(), value.begin(),
                               [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            };
            toLower(lowerName);
            toLower(lowerFilter);
            return lowerName.find(lowerFilter) != std::string::npos;
        }

        void drawEntityMetadata(const Entity &entity)
        {
            std::array<char, 128> nameBuffer{};
            copyNameToBuffer(entity.getName(), nameBuffer);

            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::InputTextWithHint("##entityName", "Entity name", nameBuffer.data(), nameBuffer.size()))
            {
                entity.setName(nameBuffer.data());
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
            {
                ImGui::SetTooltip("Entity index %u", entity.handle().index);
            }

            ImGui::Spacing();
        }

        void drawTexturePicker(MaterialRegistry *materialRegistry)
        {
            if (icons == nullptr)
                return;

            if (!texturePicker.draw(*icons))
                return;

            if (materialRegistry == nullptr)
                return;

            if (Material *material = materialRegistry->getMaterial(texturePicker.requestedMaterial()))
            {
                assignTexture(*material, texturePicker.requestedType(), texturePicker.acceptedPath());
            }
        }

        struct ModelMaterialUsage
        {
            MaterialHandle handle{};
            std::vector<size_t> submeshIndices;
        };

        // Materials that came in with the model, grouped by the submeshes that
        // use them. Scoped to the mesh node selected in the hierarchy, if any.
        void drawModelMaterials(const Entity &entity,
                                uint32_t selectedMeshNodeIndex,
                                const ComponentDrawContext &context)
        {
            if (context.materials == nullptr)
                return;

            auto *mesh = entity.tryGet<MeshRendererComponent>();
            if (mesh == nullptr)
                return;

            const Model *model = context.models != nullptr ? context.models->getModel(mesh->modelHandle) : nullptr;
            if (model == nullptr)
                return;

            const auto &allSubmeshes = model->getSubmeshes();
            const auto &meshNodes = model->getMeshNodes();

            std::vector<uint32_t> activeSubmeshIndices;
            std::string sectionLabel = "Model Materials";
            if (selectedMeshNodeIndex != Model::kInvalidNodeIndex && selectedMeshNodeIndex < meshNodes.size())
            {
                collectNodeSubmeshIndices(meshNodes, selectedMeshNodeIndex, activeSubmeshIndices);
                const auto &node = meshNodes[selectedMeshNodeIndex];
                sectionLabel = node.name.empty() ? ("Mesh Node " + std::to_string(selectedMeshNodeIndex)) : node.name;
            }
            else
            {
                activeSubmeshIndices.reserve(allSubmeshes.size());
                for (uint32_t i = 0; i < static_cast<uint32_t>(allSubmeshes.size()); ++i)
                    activeSubmeshIndices.push_back(i);
            }

            std::vector<ModelMaterialUsage> usages;
            std::unordered_map<uint32_t, size_t> usageIndexByHandle;
            for (uint32_t submeshIdx : activeSubmeshIndices)
            {
                if (submeshIdx >= allSubmeshes.size())
                    continue;
                const auto &submesh = allSubmeshes[submeshIdx];
                if (!submesh.materialHandle.isValid())
                    continue;
                auto [it, inserted] = usageIndexByHandle.try_emplace(submesh.materialHandle.value, usages.size());
                if (inserted)
                    usages.push_back(ModelMaterialUsage{submesh.materialHandle, {}});
                usages[it->second].submeshIndices.push_back(submeshIdx);
            }

            if (usages.empty())
                return;

            const EditorUI::ComponentCard card = EditorUI::beginComponentCard(sectionLabel.c_str(), false);
            if (card.open)
            {
                if (mesh->materialHandle.isValid())
                {
                    ImGui::TextDisabled("Overridden by the Mesh Renderer's material.");
                }

                for (size_t usageIndex = 0; usageIndex < usages.size(); ++usageIndex)
                {
                    const ModelMaterialUsage &usage = usages[usageIndex];
                    const std::string label = "Material " + std::to_string(usageIndex);
                    drawMaterialEntry(label.c_str(), usage.handle, context.materials,
                                      formatSubmeshList(usage.submeshIndices), context.thumbnails,
                                      context.materialTemplates, context.texturePicker);
                }
            }
            EditorUI::endComponentCard(card);
        }

        std::array<char, 64> addComponentFilter{};
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

        // One icon upload shared by every file view (asset explorer grid,
        // inspector texture picker).
        for (const auto &panel : panels)
        {
            panel->setIconLibrary(&icons);
        }
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