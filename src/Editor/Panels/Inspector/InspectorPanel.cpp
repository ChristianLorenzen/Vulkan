#include "Editor/Panels/Inspector/InspectorPanel.hpp"

#include "Assets/ModelRegistry.hpp"
#include "Core/ECS/World.hpp"
#include "Editor/Widgets/EditorWidgets.hpp"
#include "Editor/Panels/Inspector/MaterialEditor.hpp"
#include "Renderer/Material/MaterialRegistry.hpp"
#include "Renderer/Resources/Model.hpp"

#include "imgui.h"

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace Faye::Editor::Panels
{
    namespace
    {
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
    }

    void InspectorPanel::draw(ImGuiFrameData &frameData,
                              Scene *scene,
                              Entity &selectedEntity,
                              uint32_t &selectedMeshNodeIndex,
                              MaterialRegistry *materialRegistry,
                              ModelRegistry *modelRegistry,
                              const TextureThumbnailCallback *textureThumbnailCallback,
                              MaterialTemplateRegistry *materialTemplateRegistry)
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
                const Utility::ComponentDrawContext context{
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

    void InspectorPanel::drawEntityMetadata(const Entity &entity)
    {
        std::array<char, 128> nameBuffer{};
        Widgets::copyNameToBuffer(entity.getName(), nameBuffer);

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

    // Iterating the registry means a newly registered component type shows up
    // here (and in the add menu) with zero editor edits.
    void InspectorPanel::drawComponents(Scene &scene, const Utility::ComponentDrawContext &context)
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
            const Widgets::ComponentCard card = Widgets::beginComponentCard(info.name, removable);
            if (card.open)
            {
                drawers.draw(info.id, context, component);
            }
            Widgets::endComponentCard(card);
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

    void InspectorPanel::drawModelMaterials(const Entity &entity,
                                            uint32_t selectedMeshNodeIndex,
                                            const Utility::ComponentDrawContext &context)
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

        struct ModelMaterialUsage
        {
            MaterialHandle handle{};
            std::vector<size_t> submeshIndices;
        };

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

        const Widgets::ComponentCard card = Widgets::beginComponentCard(sectionLabel.c_str(), false);
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
        Widgets::endComponentCard(card);
    }

    void InspectorPanel::drawAddComponentMenu(Scene &scene, const Entity &entity)
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

    void InspectorPanel::drawTexturePicker(MaterialRegistry *materialRegistry)
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

    bool InspectorPanel::matchesFilter(const char *name) const
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
}
