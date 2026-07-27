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
