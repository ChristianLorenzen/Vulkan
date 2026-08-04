#include "Editor/Panels/Inspector/ComponentDrawers.hpp"

#include "Assets/ModelRegistry.hpp"
#include "Editor/Widgets/EditorWidgets.hpp"
#include "Editor/Panels/Inspector/MaterialEditor.hpp"
#include "Renderer/Material/MaterialRegistry.hpp"
#include "Renderer/PostProcess/PostProcessEffectLibrary.hpp"
#include "Renderer/Resources/Model.hpp"
#include "Scripting/LuaScriptSystem.hpp"
#include "Scripting/ScriptComponents.hpp"

#include "imgui.h"

#include <glm/trigonometric.hpp>

#include <algorithm>
#include <filesystem>
#include <string>

namespace Faye::Editor::Panels
{
    // ---- Component drawers -------------------------------------------
    // One free function per component type, registered in the draw table
    // below. The inspector loop draws the card header (from the type
    // registry's name) and the remove button; drawers only draw widgets.
    // Widget labels are "##hidden" — the visible label is the property row's.
    namespace
    {
        const char *effectDisplayName(const PostProcessEffectComponent &effect)
        {
            if (const auto *definition = findPostProcessEffectDefinition(effect.definitionId))
            {
                return definition->displayName.c_str();
            }

            return effect.definitionId.c_str();
        }

        void drawTransform(const Utility::ComponentDrawContext &, TransformComponent &transform)
        {
            if (!Widgets::beginProperties("##transform"))
                return;

            Widgets::propertyLabel("Position");
            ImGui::DragFloat3("##position", &transform.translation.x, 0.05f);

            // Stored in radians; shown in degrees because nobody authors rotations
            // in radians. The round-trip is lossy only below float precision.
            Widgets::propertyLabel("Rotation", "Degrees. Stored internally as radians.");
            glm::vec3 rotationDegrees = glm::degrees(transform.rotation);
            if (ImGui::DragFloat3("##rotation", &rotationDegrees.x, 0.5f))
            {
                transform.rotation = glm::radians(rotationDegrees);
            }

            Widgets::propertyLabel("Scale");
            ImGui::DragFloat3("##scale", &transform.scale.x, 0.05f, 0.01f, 100.0f);

            Widgets::endProperties();
        }

        void drawMesh(const Utility::ComponentDrawContext &context, MeshRendererComponent &mesh)
        {
            if (Widgets::beginProperties("##mesh"))
            {
                Widgets::propertyLabel("Visible");
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
                Widgets::propertyText("Model", modelName.c_str(), modelTooltip.c_str());

                if (context.materials != nullptr)
                {
                    Widgets::propertyLabel(
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

                Widgets::endProperties();
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

        void drawCamera(const Utility::ComponentDrawContext &context, CameraComponent &camera)
        {
            if (!Widgets::beginProperties("##camera"))
                return;

            Widgets::propertyLabel("Primary", "The camera the runtime renders through. Only one can be primary.");
            bool primary = camera.primary;
            if (ImGui::Checkbox("##primary", &primary) && primary)
            {
                // Clearing the flag directly would leave the scene with no camera,
                // so only promotion is offered; the scene demotes the previous one.
                context.entity.setPrimaryCamera();
            }

            Widgets::endProperties();
        }

        void drawPointLight(const Utility::ComponentDrawContext &, PointLightComponent &pointLight)
        {
            if (!Widgets::beginProperties("##pointLight"))
                return;

            Widgets::propertyLabel("Color");
            ImGui::ColorEdit3("##color", &pointLight.color.x);

            Widgets::propertyLabel("Intensity");
            ImGui::DragFloat("##intensity", &pointLight.intensity, 0.05f, 0.0f, 100.0f);

            Widgets::propertyLabel("Radius");
            ImGui::DragFloat("##radius", &pointLight.radius, 0.01f, 0.01f, 10.0f);

            Widgets::endProperties();
        }

        void drawDirectionalLight(const Utility::ComponentDrawContext &, DirectionalLightComponent &light)
        {
            if (!Widgets::beginProperties("##directionalLight"))
                return;

            Widgets::propertyLabel("Color");
            ImGui::ColorEdit3("##color", &light.color.x);

            Widgets::propertyLabel("Intensity");
            ImGui::DragFloat("##intensity", &light.intensity, 0.05f, 0.0f, 100.0f);

            // Direction comes from the entity's Transform rotation — rotate the
            // entity to aim the sun.
            Widgets::propertyText("Direction", "From Transform rotation",
                                   "Rotate the entity's Transform to aim this light.");

            Widgets::endProperties();
        }

        void drawWater(const Utility::ComponentDrawContext &, WaterComponent &water)
        {
            if (!Widgets::beginProperties("##water"))
                return;

            Widgets::propertyLabel("Subdivisions",
                                    "Applied automatically by the WaterSubdivision script each frame.");
            int divs = static_cast<int>(water.subdivisions);
            if (ImGui::DragInt("##subdivisions", &divs, 1.0f, 4, 256, "%d"))
            {
                water.subdivisions = static_cast<uint32_t>(std::clamp(divs, 4, 256));
            }

            Widgets::endProperties();
        }

        void drawPostProcessStack(const Utility::ComponentDrawContext &, PostProcessStackComponent &postProcessStack)
        {
            if (Widgets::beginProperties("##postProcess"))
            {
                Widgets::propertyLabel("Enabled");
                ImGui::Checkbox("##enabled", &postProcessStack.enabled);
                Widgets::endProperties();
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

                    if (Widgets::beginProperties("##effect"))
                    {
                        Widgets::propertyLabel("Type");
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

                        Widgets::propertyLabel("Enabled");
                        ImGui::Checkbox("##effectEnabled", &effect.enabled);

                        if (effectDefinition == nullptr)
                        {
                            Widgets::propertyText("Definition", "missing");
                        }
                        else
                        {
                            for (const auto &parameter : effectDefinition->parameters)
                            {
                                Widgets::propertyLabel(parameter.label.c_str());
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

                        Widgets::endProperties();
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
        void drawNativeScript(const Utility::ComponentDrawContext &context, NativeScriptComponent &script)
        {
            if (!Widgets::beginProperties("##nativeScript"))
                return;

            Widgets::propertyText("Script",
                                   script.scriptName.empty() ? "<unbound>" : script.scriptName.c_str(),
                                   script.scriptPath.empty() ? nullptr : script.scriptPath.c_str());

            Widgets::endProperties();
        }

        void drawLuaScript(const Utility::ComponentDrawContext &, LuaScriptComponent &script)
        {
            if (!Widgets::beginProperties("##luaScript"))
                return;

            const std::string name = std::filesystem::path(script.scriptPath).stem().string();
            Widgets::propertyText("Script",
                                   name.empty() ? "<unbound>" : name.c_str(),
                                   script.scriptPath.empty() ? nullptr : script.scriptPath.c_str());

            Widgets::endProperties();
        }
    }

    Utility::ComponentDrawRegistry makeEditorDrawRegistry()
    {
        Utility::ComponentDrawRegistry registry;
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
}
