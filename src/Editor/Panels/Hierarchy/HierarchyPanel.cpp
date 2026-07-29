#include "Editor/Panels/Hierarchy/HierarchyPanel.hpp"

#include "Assets/ModelRegistry.hpp"
#include "Renderer/Resources/Model.hpp"

#include "imgui.h"

#include <string>
#include <vector>

namespace Faye::Editor::Panels
{
    namespace
    {
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

    }

    bool HierarchyPanel::matchesFilter(const char *name) const
    {
        if (entityFilter[0] == '\0')
            return true;

        // Case-insensitive substring match, so "light" finds "Point Light".
        std::string lowerName(name);
        std::string lowerFilter(entityFilter.data());
        const auto toLower = [](std::string &value) {
            std::transform(value.begin(), value.end(), value.begin(),
                        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        };
        toLower(lowerName);
        toLower(lowerFilter);
        return lowerName.find(lowerFilter) != std::string::npos;
    }

    void HierarchyPanel::draw(ImGuiFrameData &frameData,
                              Scene *scene,
                              Entity &selectedEntity,
                              uint32_t &selectedMeshNodeIndex,
                              MaterialRegistry *materialRegistry,
                              ModelRegistry *modelRegistry,
                              const TextureThumbnailCallback *textureThumbnailCallback,
                              MaterialTemplateRegistry *materialTemplateRegistry)
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
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                ImGui::InputTextWithHint("##heirarchyfilter", "Filter", entityFilter.data(), entityFilter.size());
                ImGui::Separator();

                for (Ecs::Entity entityHandle : scene->getEntities())
                {
                    Entity entity = scene->getEntity(entityHandle);
                    const std::string_view name = entity.getName();

                    if (!matchesFilter(name.data()))
                        continue;

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

                    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(15.0f, 15.0f));
                    if (ImGui::BeginPopupContextItem())
                    {
                        if (ImGui::MenuItem("Delete"))
                        {
                            this->entityContext = { entityHandle, ContextType::deleteEntity };
                        }
                        if (ImGui::MenuItem("Duplicate"))
                        {
                            this->entityContext = { entityHandle, ContextType::duplicateEntity };
                        }
                        ImGui::EndPopup();
                    }
                    ImGui::PopStyleVar();

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

        if (this->entityContext)
        {
            if (this->entityContext->contextType == ContextType::deleteEntity)
            {
                scene->destroyEntity(this->entityContext->contextEntity);
            }
            else if (this->entityContext->contextType == ContextType::duplicateEntity)
            {
                scene->duplicateEntity(this->entityContext->contextEntity);
            }
            this->entityContext = {};
        }
    }

    void HierarchyPanel::drawMeshNodeTree(const Entity &entity,
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
}
