#include "Editor/ImGui/EditorPanels.hpp"

#include "Assets/ModelRegistry.hpp"
#include "Renderer/Material/MaterialRegistry.hpp"
#include "Renderer/Material/MaterialTemplate.hpp"
#include "Renderer/PostProcess/PostProcessEffectLibrary.hpp"
#include "Renderer/Resources/Model.hpp"
#include "Scripting/ScriptSystem.hpp"

#include "imgui.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <sstream>
#include <string>
#include <unordered_map>

namespace Faye
{
    namespace
    {
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
                                ImGui::Text("Current: %s", viewportDebugModeLabel(frameData.viewportDebugMode));
                                ImGui::EndPopup();
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
                        for (Scene::EntityId entityId : scene->getEntities())
                        {
                            Entity entity = scene->getEntity(entityId);
                            const std::string_view name = entity.getName();

                            const Model *model = nullptr;
                            if (modelRegistry != nullptr)
                            {
                                if (auto *meshComp = entity.tryGetMesh())
                                    model = modelRegistry->getModel(meshComp->modelHandle);
                            }

                            const bool hasNodeTree = model != nullptr &&
                                                     model->getRootNodeIndex() != Model::kInvalidNodeIndex &&
                                                     nodeHasGeometry(model->getMeshNodes(), model->getRootNodeIndex());

                            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow |
                                                       ImGuiTreeNodeFlags_SpanAvailWidth;
                            if (!hasNodeTree)
                                flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

                            const bool entitySelected = entity.id() == selectedEntity.id() &&
                                                        selectedMeshNodeIndex == Model::kInvalidNodeIndex;
                            if (entitySelected)
                                flags |= ImGuiTreeNodeFlags_Selected;

                            const char *displayName = name.empty() ? "<unnamed>" : name.data();
                            ImGui::PushID(static_cast<int>(entityId));

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

                const bool isSelected = selectedEntity.id() == entity.id() &&
                                        selectedMeshNodeIndex == nodeIndex;

                const bool hasChildren = [&]() {
                    for (uint32_t childIdx : node.childNodeIndices)
                        if (nodeHasGeometry(nodes, childIdx)) return true;
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
                        drawAttachedComponents(selectedEntity);
                        drawTransform(selectedEntity);
                        drawMesh(selectedEntity);
                        drawMaterial(selectedEntity, selectedMeshNodeIndex, materialRegistry, modelRegistry, textureThumbnailCallback, materialTemplateRegistry);
                        drawCamera(selectedEntity);
                        drawPointLight(selectedEntity);
                        drawPostProcessStack(selectedEntity);
                        drawScript(selectedEntity);
                    }
                }
                ImGui::End();
            }

            void bindScriptSystem(ScriptSystem *sys) override { scriptSystem = sys; }

        private:
            ScriptSystem *scriptSystem = nullptr;
            void drawEntityMetadata(const Entity &entity)
            {
                std::array<char, 128> nameBuffer{};
                copyNameToBuffer(entity.getName(), nameBuffer);

                if (ImGui::InputText("Name", nameBuffer.data(), nameBuffer.size()))
                {
                    entity.setName(nameBuffer.data());
                }

                ImGui::Text("Entity ID: %u", entity.id());
                ImGui::Separator();
            }

            void drawAttachedComponents(const Entity &entity)
            {
                const auto componentKinds = entity.getComponentKinds();
                if (componentKinds.empty())
                {
                    return;
                }

                ImGui::TextUnformatted("Attached Components");
                for (ComponentKind componentKind : componentKinds)
                {
                    ImGui::BulletText("%s", componentKindName(componentKind));
                }
                ImGui::Separator();
            }

            void drawTransform(const Entity &entity)
            {
                if (auto *transform = entity.tryGetTransform())
                {
                    if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
                    {
                        ImGui::DragFloat3("Translation", &transform->translation.x, 0.05f);
                        ImGui::DragFloat3("Rotation", &transform->rotation.x, 0.01f);
                        ImGui::DragFloat3("Scale", &transform->scale.x, 0.05f, 0.01f, 100.0f);
                    }
                }
            }

            void drawMesh(const Entity &entity)
            {
                if (auto *mesh = entity.tryGetMesh())
                {
                    if (ImGui::CollapsingHeader("Mesh", ImGuiTreeNodeFlags_DefaultOpen))
                    {
                        ImGui::Text("Model Handle: %u", mesh->modelHandle.value);
                        ImGui::Text("Material Handle: %u", mesh->materialHandle.value);
                    }
                }
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
                            if (desc.field == "baseColorFactor" && desc.type == ShaderProperty::Type::Vec4)
                            {
                                if (ImGui::ColorEdit4(desc.label.c_str(), &materialData.baseColorFactor.x))
                                {
                                    materialData.opacity = materialData.baseColorFactor.a;
                                    materialChanged = true;
                                }
                            }
                            else if (desc.field == "metallicFactor" && desc.type == ShaderProperty::Type::Float)
                            {
                                materialChanged |= ImGui::DragFloat(desc.label.c_str(), &materialData.metallicFactor,
                                                                    0.01f, desc.minVal, desc.maxVal);
                            }
                            else if (desc.field == "roughnessFactor" && desc.type == ShaderProperty::Type::Float)
                            {
                                materialChanged |= ImGui::DragFloat(desc.label.c_str(), &materialData.roughnessFactor,
                                                                    0.01f, desc.minVal, desc.maxVal);
                            }
                            else if (desc.field == "emissive" && desc.type == ShaderProperty::Type::Vec3)
                            {
                                materialChanged |= ImGui::ColorEdit3(desc.label.c_str(), &materialData.emissive.x);
                            }
                            else if (desc.field == "emissiveIntensity" && desc.type == ShaderProperty::Type::Float)
                            {
                                materialChanged |= ImGui::DragFloat(desc.label.c_str(), &materialData.emissiveIntensity,
                                                                    0.1f, desc.minVal, desc.maxVal);
                            }
                            else if (desc.field == "shininess" && desc.type == ShaderProperty::Type::Float)
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

                auto *mesh = entity.tryGetMesh();
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

            void drawCamera(const Entity &entity)
            {
                if (auto *camera = entity.tryGetCamera())
                {
                    if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen))
                    {
                        ImGui::TextUnformatted(camera->primary ? "Primary Camera" : "Camera");
                        if (!camera->primary && ImGui::Button("Set As Primary"))
                        {
                            entity.setPrimaryCamera();
                        }
                    }
                }
            }

            void drawPointLight(const Entity &entity)
            {
                if (auto *pointLight = entity.tryGetPointLight())
                {
                    if (ImGui::CollapsingHeader("Point Light", ImGuiTreeNodeFlags_DefaultOpen))
                    {
                        ImGui::ColorEdit3("Light Color", &pointLight->color.x);
                        ImGui::DragFloat("Intensity", &pointLight->intensity, 0.05f, 0.0f, 100.0f);
                        ImGui::DragFloat("Radius", &pointLight->radius, 0.01f, 0.01f, 10.0f);
                    }
                }
            }

            void drawPostProcessStack(const Entity &entity)
            {
                auto *postProcessStack = entity.tryGetPostProcessStack();
                if (postProcessStack == nullptr)
                {
                    return;
                }

                if (!ImGui::CollapsingHeader("Post Process Stack", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    return;
                }

                ImGui::Checkbox("Enabled", &postProcessStack->enabled);
                ImGui::Separator();

                int moveUpIndex = -1;
                int moveDownIndex = -1;
                int removeIndex = -1;

                for (size_t i = 0; i < postProcessStack->effects.size(); ++i)
                {
                    auto &effect = postProcessStack->effects[i];
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
                        if (ImGui::Button("Move Down") && i + 1 < postProcessStack->effects.size())
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
                    postProcessStack->effects.erase(postProcessStack->effects.begin() + removeIndex);
                }
                else if (moveUpIndex > 0)
                {
                    std::swap(postProcessStack->effects[moveUpIndex], postProcessStack->effects[moveUpIndex - 1]);
                }
                else if (moveDownIndex >= 0 && static_cast<size_t>(moveDownIndex + 1) < postProcessStack->effects.size())
                {
                    std::swap(postProcessStack->effects[moveDownIndex], postProcessStack->effects[moveDownIndex + 1]);
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
                            postProcessStack->effects.push_back(makeDefaultPostProcessEffect(definition.id));
                        }
                    }
                    ImGui::EndCombo();
                }
            }

            bool open = true;

            void drawScript(const Entity &entity)
            {
                if (scriptSystem == nullptr)
                    return;

                const ScriptComponent *script = scriptSystem->tryGetScriptComponent(entity.id());
                if (script == nullptr)
                    return;

                if (ImGui::CollapsingHeader("Script", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    ImGui::Text("Name: %s", script->scriptName.c_str());
                    ImGui::TextWrapped("Path: %s", script->scriptPath.c_str());
                }
            }
        };
    }

    EditorPanels::EditorPanels()
    {
        panels.push_back(std::make_unique<FrameStatsPanel>());
        panels.push_back(std::make_unique<EditorViewPanel>());
        panels.push_back(std::make_unique<HierarchyPanel>());
        panels.push_back(std::make_unique<InspectorPanel>());
        panels.push_back(std::make_unique<RuntimeViewPanel>());
    }

    EditorPanels::~EditorPanels() = default;

    void EditorPanels::draw(ImGuiFrameData &frameData)
    {
        drawDockspace();

        for (const auto &panel : panels)
        {
            if (!panel->isOpen())
                continue;

            panel->bindScriptSystem(scriptSystem);
            panel->draw(frameData, boundScene, selectedEntity, selectedMeshNodeIndex, materialRegistry, modelRegistry, &textureThumbnailCallback, materialTemplateRegistry);
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