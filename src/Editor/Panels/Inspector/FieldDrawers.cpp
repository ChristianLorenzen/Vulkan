#include "Editor/Panels/Inspector/FieldDrawers.hpp"

#include "engine/Assets/ModelRegistry.hpp"
#include "Core/ECS/World.hpp"
#include "Core/Serialization/Uuid.hpp"
#include "Editor/Panels/Inspector/MaterialEditor.hpp"
#include "Editor/Utility/FieldDrawRegistry.hpp"
#include "Editor/Widgets/EditorWidgets.hpp"
#include "Renderer/Material/MaterialRegistry.hpp"
#include "Renderer/PostProcess/PostProcessEffectLibrary.hpp"
#include "Renderer/Resources/Model.hpp"

#include "imgui.h"

#include <glm/glm.hpp>
#include <glm/trigonometric.hpp>

#include <array>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

// The reflected inspector. Every widget here is chosen from a FieldDescriptor
// alone -- a semantic TypeId if the field has one, otherwise its Layout -- so
// adding a component type to the engine gives it an inspector for free.
//
// Two rules keep this honest:
//
//   * A drawer returns true ONLY when it edited the value this frame. That
//     return value is what fires TypeDescriptor::onFieldChanged, so a drawer
//     that mutates silently skips invariant repair.
//   * A field that resolves to no drawer renders nothing at all. That is the
//     right answer for CameraComponent::camera (Layout::Opaque, no semantic):
//     the engine owns it, the user has no business editing it, and a
//     "(unsupported)" row would just be noise in every camera card.
namespace Faye::Editor::Panels
{
    namespace
    {
        using Utility::FieldDrawContext;

        // ---- Shared helpers ------------------------------------------------

        // "modelHandle" -> "Model Handle". Field names come from the C++
        // identifier, and lowerCamelCase reads badly as a row label.
        std::string prettifyName(const Ecs::FieldDescriptor &field)
        {
            const char *name = field.name != nullptr ? field.name : "field";

            std::string label;
            for (const char *c = name; *c != '\0'; ++c)
            {
                const auto character = static_cast<unsigned char>(*c);
                if (c == name)
                    label += static_cast<char>(std::toupper(character));
                else if (*c == '_')
                    label += ' ';
                else if (std::isupper(character) != 0)
                {
                    label += ' ';
                    label += *c;
                }
                else
                    label += *c;
            }
            return label;
        }

        // Memoised, because the inspector redraws every field of every
        // component every frame and the hand-written drawers it replaces pass
        // string literals. Keyed on the descriptor's address, which is safe
        // precisely because kDescriptor<T> and kElementDescriptor<T> are inline
        // constexpr variables: one per type, static storage, address-stable
        // across translation units (see Describe.hpp).
        const char *displayName(const Ecs::FieldDescriptor &field)
        {
            static std::unordered_map<const Ecs::FieldDescriptor *, std::string> cache;

            auto it = cache.find(&field);
            if (it == cache.end())
                it = cache.emplace(&field, prettifyName(field)).first;
            return it->second.c_str();
        }

        // Appends the field's unit suffix to an ImGui format string. A '%' in
        // the unit has to be doubled or ImGui reads it as another conversion,
        // which is not hypothetical: "%" is a plausible unit.
        const char *formatWithUnits(const Ecs::FieldDescriptor &field, const char *base,
                                    char *buffer, size_t capacity)
        {
            if (field.ui.units == nullptr || *field.ui.units == '\0')
                return base;

            size_t next = 0;
            const auto append = [&](char c) {
                if (next + 1 < capacity)
                    buffer[next++] = c;
            };

            for (const char *c = base; *c != '\0'; ++c)
                append(*c);
            append(' ');
            for (const char *c = field.ui.units; *c != '\0'; ++c)
            {
                if (*c == '%')
                    append('%');
                append(*c);
            }

            buffer[next] = '\0';
            return buffer;
        }

        // ---- Layout drawers ------------------------------------------------

        bool drawBool(const FieldDrawContext &context)
        {
            return ImGui::Checkbox("##value", static_cast<bool *>(context.value));
        }

        template <int DataType>
        constexpr const char *defaultFormat()
        {
            // ImGui formats through its own type-aware printf, so the
            // conversion has to match the data type exactly -- "%d" on an S64
            // prints garbage rather than being merely imprecise.
            if constexpr (DataType == ImGuiDataType_Float || DataType == ImGuiDataType_Double)
                return "%.3f";
            else if constexpr (DataType == ImGuiDataType_S64)
                return "%lld";
            else if constexpr (DataType == ImGuiDataType_U64)
                return "%llu";
            else if constexpr (DataType == ImGuiDataType_U8 || DataType == ImGuiDataType_U16 ||
                               DataType == ImGuiDataType_U32)
                return "%u";
            else
                return "%d";
        }

        // One template covers every scalar layout: the only per-type facts are
        // the C++ type, ImGui's tag for it, and the drag speed. A Range
        // annotation turns the drag into a slider, which is exactly what
        // WaterComponent::subdivisions wants (4..256) and what drawWater
        // currently hardcodes.
        bool drawEnumCombo(const FieldDrawContext &context);

        template <class T, int DataType, bool IsFloat>
        bool drawNumber(const FieldDrawContext &context)
        {
            T *value = static_cast<T *>(context.value);
            const Ecs::FieldUI &ui = context.field->ui;

            // An enum reaches here as its underlying integer layout -- that is
            // how it is stored. What it MEANS is a named set, so it is drawn as
            // one.
            if (context.field->enumOps != nullptr)
                return drawEnumCombo(context);

            char buffer[64];
            const char *format =
                formatWithUnits(*context.field, defaultFormat<DataType>(), buffer, sizeof buffer);

            if (ui.hasRange)
            {
                const T low = static_cast<T>(ui.rangeMin);
                const T high = static_cast<T>(ui.rangeMax);
                return ImGui::SliderScalar("##value", DataType, value, &low, &high, format);
            }

            return ImGui::DragScalar("##value", DataType, value, IsFloat ? 0.05f : 1.0f,
                                     nullptr, nullptr, format);
        }

        // Shared by vec2/3/4: the range annotation, if present, clamps every
        // component. ImGui wants speed/min/max as floats here, not as T.
        template <int Components>
        bool dragVector(const FieldDrawContext &context, float *value, float speed)
        {
            const Ecs::FieldUI &ui = context.field->ui;

            char buffer[64];
            const char *format = formatWithUnits(*context.field, "%.3f", buffer, sizeof buffer);
            const float low = ui.hasRange ? ui.rangeMin : 0.0f;
            const float high = ui.hasRange ? ui.rangeMax : 0.0f;

            return ImGui::DragScalarN("##value", ImGuiDataType_Float, value, Components, speed,
                                      ui.hasRange ? &low : nullptr, ui.hasRange ? &high : nullptr,
                                      format);
        }

        bool drawVec2(const FieldDrawContext &context)
        {
            return dragVector<2>(context, &static_cast<glm::vec2 *>(context.value)->x, 0.05f);
        }

        bool drawVec3(const FieldDrawContext &context)
        {
            glm::vec3 *value = static_cast<glm::vec3 *>(context.value);

            if (context.field->ui.colorPicker)
                return ImGui::ColorEdit3("##value", &value->x);

            // Stored in radians, edited in degrees -- nobody authors rotations
            // in radians. The round trip is lossy only below float precision.
            if (context.field->ui.radians)
            {
                glm::vec3 degrees = glm::degrees(*value);
                if (!ImGui::DragFloat3("##value", &degrees.x, 0.5f))
                    return false;
                *value = glm::radians(degrees);
                return true;
            }

            return dragVector<3>(context, &value->x, 0.05f);
        }

        bool drawVec4(const FieldDrawContext &context)
        {
            glm::vec4 *value = static_cast<glm::vec4 *>(context.value);

            if (context.field->ui.colorPicker)
                return ImGui::ColorEdit4("##value", &value->x);

            return dragVector<4>(context, &value->x, 0.05f);
        }

        // Collects the field's current option set. Reused across fields and
        // frames: providers are called every frame by design, and this keeps
        // that from being an allocation each time.
        const std::vector<Ecs::FieldOption> &collectOptions(const Ecs::FieldDescriptor &field)
        {
            static std::vector<Ecs::FieldOption> options;
            options.clear();
            if (field.ui.options != nullptr)
            {
                field.ui.options(Ecs::OptionSink{
                    &options,
                    [](void *sink, Ecs::FieldOption option) {
                        static_cast<std::vector<Ecs::FieldOption> *>(sink)->push_back(option);
                    }});
            }
            return options;
        }

        // An enum. No annotation is involved and no provider is called: the
        // name/value table already hangs off the descriptor because the
        // SERIALIZER needs it too, and one table feeding both is what keeps the
        // dropdown and the save file from ever disagreeing.
        bool drawEnumCombo(const FieldDrawContext &context)
        {
            const Ecs::EnumOps &ops = *context.field->enumOps;
            const int64_t current = ops.read(context.value);

            const char *preview = ops.nameOf(current);
            char unnamed[32];
            if (preview == nullptr)
            {
                // A value from a newer build, or an int cast in. Show it rather
                // than pretending it is the first enumerator.
                std::snprintf(unnamed, sizeof unnamed, "(%lld)",
                              static_cast<long long>(current));
                preview = unnamed;
            }

            bool edited = false;
            if (ImGui::BeginCombo("##value", preview))
            {
                for (uint32_t i = 0; i < ops.count; ++i)
                {
                    const Ecs::EnumEntry &entry = ops.entries[i];
                    const bool selected = entry.value == current;
                    if (ImGui::Selectable(entry.name, selected) && !selected)
                    {
                        ops.write(context.value, entry.value);
                        edited = true;
                    }
                    if (selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            return edited;
        }

        // A string constrained to a set the runtime computes. The provider came
        // straight out of the field's OptionsFrom annotation -- there is no name
        // to resolve, so the only way to reach here with a null provider is to
        // not have annotated the field at all.
        bool drawOptionsCombo(const FieldDrawContext &context, std::string *value)
        {
            if (context.field->ui.options == nullptr)
                return false;

            const auto &options = collectOptions(*context.field);

            const char *preview = value->c_str();
            for (const Ecs::FieldOption &option : options)
            {
                if (option.value != nullptr && *value == option.value)
                {
                    preview = option.label != nullptr ? option.label : option.value;
                    break;
                }
            }

            bool edited = false;
            if (ImGui::BeginCombo("##value", preview))
            {
                for (const Ecs::FieldOption &option : options)
                {
                    if (option.value == nullptr)
                        continue;
                    const bool selected = *value == option.value;
                    const char *label = option.label != nullptr ? option.label : option.value;
                    if (ImGui::Selectable(label, selected) && !selected)
                    {
                        *value = option.value;
                        edited = true;
                    }
                    if (selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            return edited;
        }

        bool drawString(const FieldDrawContext &context)
        {
            std::string *value = static_cast<std::string *>(context.value);

            if (context.field->ui.options != nullptr)
                return drawOptionsCombo(context, value);

            // No imgui_stdlib in this build, so the edit goes through a fixed
            // buffer -- the same 128-byte convention the entity-name and
            // material-name rows already use. copyNameToBuffer truncates rather
            // than overflowing, and the write-back only happens on an actual
            // edit, so merely looking at an over-long value cannot destroy it.
            std::array<char, 128> buffer{};
            Widgets::copyNameToBuffer(*value, buffer);

            if (!ImGui::InputText("##value", buffer.data(), buffer.size()))
                return false;

            *value = buffer.data();
            return true;
        }

        // ---- Semantic drawers ----------------------------------------------

        bool drawModelAsset(const FieldDrawContext &context)
        {
            // Read-only: ModelRegistry has no enumeration API, so there is no
            // list to pick from. Models are assigned by dropping an asset on
            // the entity, and this row reports the result -- the same thing
            // drawMesh does today.
            const ModelHandle handle = *static_cast<const ModelHandle *>(context.value);
            const ModelRegistry *models = context.owner->models;
            const Model *model = models != nullptr ? models->getModel(handle) : nullptr;

            // No std::string: this runs every frame, and the node name is
            // already a stable string owned by the model.
            const char *name = "None";
            char fallback[32];
            if (model != nullptr)
            {
                const auto &nodes = model->getMeshNodes();
                const uint32_t root = model->getRootNodeIndex();
                if (root < nodes.size() && !nodes[root].name.empty())
                {
                    name = nodes[root].name.c_str();
                }
                else
                {
                    std::snprintf(fallback, sizeof fallback, "Model %u", handle.value);
                    name = fallback;
                }
            }

            ImGui::TextUnformatted(name);
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                ImGui::SetTooltip("Model handle %u", handle.value);
            return false;
        }

        bool drawMaterialAsset(const FieldDrawContext &context)
        {
            MaterialHandle *handle = static_cast<MaterialHandle *>(context.value);
            MaterialRegistry *materials = context.owner->materials;
            if (materials == nullptr)
            {
                ImGui::TextDisabled("(no material registry)");
                return false;
            }

            const Material *current = materials->getMaterial(*handle);
            const char *preview = current != nullptr && !current->getName().empty()
                                      ? current->getName().c_str()
                                      : (handle->isValid() ? "Unnamed Material" : "None");

            // Reserve room for the "New" button so the combo does not run under
            // it when the panel is narrow. propertyLabel already stretched the
            // item to the full cell, so this has to override it.
            const float newButtonWidth =
                ImGui::CalcTextSize("New").x + ImGui::GetStyle().FramePadding.x * 2.0f;
            ImGui::SetNextItemWidth(-(newButtonWidth + ImGui::GetStyle().ItemSpacing.x));

            bool edited = false;
            if (ImGui::BeginCombo("##value", preview))
            {
                if (ImGui::Selectable("None", !handle->isValid()))
                {
                    *handle = {};
                    edited = true;
                }

                for (const MaterialHandle candidate : materials->getAllHandles())
                {
                    const Material *material = materials->getMaterial(candidate);
                    const std::string label =
                        (material != nullptr && !material->getName().empty() ? material->getName()
                                                                            : std::string("Unnamed Material")) +
                        "##" + std::to_string(candidate.value);
                    if (ImGui::Selectable(label.c_str(), candidate.value == handle->value))
                    {
                        *handle = candidate;
                        edited = true;
                    }
                }
                ImGui::EndCombo();
            }

            ImGui::SameLine();
            if (ImGui::Button("New"))
            {
                MaterialData created{};
                created.name = "New Material";
                *handle = materials->registerMaterial(std::move(created));
                edited = true;
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                ImGui::SetTooltip("Register a new default PBR material and assign it");

            return edited;
        }

        // The assigned material, edited in place directly beneath the row that
        // assigned it. Registered as an EXPANSION rather than folded into
        // drawMaterialAsset because drawMaterialProperties opens its own tables
        // and sections, and ImGui tables do not nest -- the walker runs this
        // after the property table closes.
        bool drawMaterialExpansion(const FieldDrawContext &context)
        {
            const MaterialHandle handle = *static_cast<const MaterialHandle *>(context.value);
            MaterialRegistry *materials = context.owner->materials;
            if (materials == nullptr || !handle.isValid())
                return false;

            Material *material = materials->getMaterial(handle);
            if (material == nullptr)
                return false;

            drawMaterialProperties(handle, *material, {}, context.owner->thumbnails,
                                   context.owner->materialTemplates, context.owner->texturePicker);

            // Edits land on the material asset, not on the handle field, so
            // this must not report the FIELD as changed -- doing so would fire
            // the component's onFieldChanged for something it did not touch.
            return false;
        }

        bool drawUuid(const FieldDrawContext &context)
        {
            // Uuid has private members, so it stays Layout::Opaque and would
            // otherwise render nothing. Identity is worth showing; it is never
            // worth editing.
            ImGui::TextDisabled("%s", static_cast<const Uuid *>(context.value)->toString().c_str());
            return false;
        }

        // ---- The table -----------------------------------------------------

        Utility::FieldDrawRegistry makeFieldDrawRegistry()
        {
            Utility::FieldDrawRegistry registry;

            registry.registerLayout(Ecs::Layout::Bool, &drawBool);
            registry.registerLayout(Ecs::Layout::I8, &drawNumber<int8_t, ImGuiDataType_S8, false>);
            registry.registerLayout(Ecs::Layout::I16, &drawNumber<int16_t, ImGuiDataType_S16, false>);
            registry.registerLayout(Ecs::Layout::I32, &drawNumber<int32_t, ImGuiDataType_S32, false>);
            registry.registerLayout(Ecs::Layout::I64, &drawNumber<int64_t, ImGuiDataType_S64, false>);
            registry.registerLayout(Ecs::Layout::U8, &drawNumber<uint8_t, ImGuiDataType_U8, false>);
            registry.registerLayout(Ecs::Layout::U16, &drawNumber<uint16_t, ImGuiDataType_U16, false>);
            registry.registerLayout(Ecs::Layout::U32, &drawNumber<uint32_t, ImGuiDataType_U32, false>);
            registry.registerLayout(Ecs::Layout::U64, &drawNumber<uint64_t, ImGuiDataType_U64, false>);
            registry.registerLayout(Ecs::Layout::F32, &drawNumber<float, ImGuiDataType_Float, true>);
            registry.registerLayout(Ecs::Layout::F64, &drawNumber<double, ImGuiDataType_Double, true>);
            registry.registerLayout(Ecs::Layout::F32x2, &drawVec2);
            registry.registerLayout(Ecs::Layout::F32x3, &drawVec3);
            registry.registerLayout(Ecs::Layout::F32x4, &drawVec4);
            registry.registerLayout(Ecs::Layout::String, &drawString);

            // F32x3x3 / F32x4x4 deliberately absent: a raw 16-float grid is not
            // an edit anyone wants, and no component exposes one. Struct and
            // DynArray are absent because they never reach resolve() -- the
            // walker handles them structurally.

            // Only where Layout genuinely cannot express the widget. faye.f32
            // and faye.bool need no entry; Layout::F32 and Layout::Bool are
            // already the right answer for them.
            registry.registerSemantic(Ecs::hashTypeName("faye.ModelAsset"), &drawModelAsset);
            registry.registerSemantic(Ecs::hashTypeName("faye.MaterialAsset"), &drawMaterialAsset);
            registry.registerSemantic(Ecs::hashTypeName("faye.Uuid"), &drawUuid);

            registry.registerSemanticExpansion(Ecs::hashTypeName("faye.MaterialAsset"),
                                               &drawMaterialExpansion);


            return registry;
        }

        const Utility::FieldDrawRegistry &fieldDrawers()
        {
            static const Utility::FieldDrawRegistry registry = makeFieldDrawRegistry();
            return registry;
        }

        // ---- The walker ----------------------------------------------------

        // The invariant-repair target for whatever leaf is being edited.
        // Always the ROOT component and the ROOT field, even when the edit is
        // several levels down inside a nested struct or a container element:
        // invariants are expressed over the component, and onFieldChanged
        // carries exactly one FieldDescriptor.
        struct FieldHook
        {
            const Ecs::TypeDescriptor *type = nullptr;
            void *object = nullptr;
            const Ecs::FieldDescriptor *field = nullptr;

            void fire(const Utility::ComponentDrawContext &context) const
            {
                if (type == nullptr || type->onFieldChanged == nullptr)
                    return;
                if (object == nullptr || field == nullptr || context.world == nullptr)
                    return;
                type->onFieldChanged(object, *field, *context.world, context.entity.handle());
            }
        };

        bool drawFieldList(const Ecs::TypeDescriptor &type, void *object,
                           const Utility::ComponentDrawContext &context, FieldHook hook);

        // A leaf: one property row, one widget. The only place a hook fires.
        bool drawLeafRow(const Ecs::FieldDescriptor &field, void *value, const char *label,
                         const Utility::ComponentDrawContext &context, const FieldHook &hook)
        {
            const Utility::FieldDrawFn draw = fieldDrawers().resolve(field);
            if (draw == nullptr)
                return false;

            const bool readOnly = hasFlag(field.flags, Ecs::FieldFlags::ReadOnly);

            Widgets::propertyLabel(label, field.ui.tooltip);
            if (readOnly)
                ImGui::BeginDisabled();
            const bool edited = draw({&context, &field, value});
            if (readOnly)
                ImGui::EndDisabled();

            // A ReadOnly widget cannot report an edit, but checking anyway keeps
            // the rule "disabled fields never fire hooks" true by construction
            // rather than by trusting BeginDisabled.
            if (edited && !readOnly)
                hook.fire(context);
            return edited && !readOnly;
        }

        // A nested struct. Property tables do not nest, so the caller has
        // already closed the run; this opens its own inside the tree node.
        bool drawNestedStruct(const Ecs::FieldDescriptor &field, void *value, const char *label,
                              const Utility::ComponentDrawContext &context, const FieldHook &hook)
        {
            if (field.nested == nullptr)
                return false;

            bool changed = false;
            if (ImGui::TreeNodeEx(label, ImGuiTreeNodeFlags_DefaultOpen))
            {
                changed = drawFieldList(*field.nested, value, context, hook);
                ImGui::TreePop();
            }
            return changed;
        }

        // An index operation chosen while drawing and applied after the loop.
        // Mutating the container mid-iteration would invalidate the element
        // pointers the rest of the loop is about to hand to ImGui.
        struct ElementCommand
        {
            int remove = -1;
            int moveUp = -1;
            int moveDown = -1;

            bool apply(const Ecs::FieldOps &ops, void *container, size_t count) const
            {
                if (remove >= 0 && ops.eraseAt != nullptr)
                {
                    ops.eraseAt(container, static_cast<size_t>(remove));
                    return true;
                }
                if (ops.swapElements == nullptr)
                    return false;
                if (moveUp > 0)
                {
                    ops.swapElements(container, static_cast<size_t>(moveUp),
                                     static_cast<size_t>(moveUp) - 1);
                    return true;
                }
                if (moveDown >= 0 && static_cast<size_t>(moveDown) + 1 < count)
                {
                    ops.swapElements(container, static_cast<size_t>(moveDown),
                                     static_cast<size_t>(moveDown) + 1);
                    return true;
                }
                return false;
            }
        };

        // Reordering is opt-in per field (Ecs::Orderable); removal is always
        // offered, because eraseAt is generic and "delete this one" is never
        // wrong for a list the user can already grow.
        bool elementControlsEnabled(const Ecs::FieldDescriptor &field, bool readOnly)
        {
            return !readOnly && field.ops != nullptr &&
                   (field.ops->eraseAt != nullptr ||
                    (field.ui.orderable && field.ops->swapElements != nullptr));
        }

        float elementControlsWidth(const Ecs::FieldDescriptor &field)
        {
            const bool orderable = field.ui.orderable && field.ops->swapElements != nullptr;
            const int buttons = (field.ops->eraseAt != nullptr ? 1 : 0) + (orderable ? 2 : 0);
            return ImGui::GetFrameHeight() * static_cast<float>(buttons);
        }

        // Draws at the cursor; the caller positions it.
        void drawElementControls(const Ecs::FieldDescriptor &field, size_t index, size_t count,
                                 ElementCommand &command)
        {
            const float width = ImGui::GetFrameHeight();
            if (field.ui.orderable && field.ops->swapElements != nullptr)
            {
                ImGui::BeginDisabled(index == 0);
                if (ImGui::Button("^", ImVec2(width, 0.0f)))
                    command.moveUp = static_cast<int>(index);
                ImGui::EndDisabled();
                ImGui::SameLine(0.0f, 0.0f);

                ImGui::BeginDisabled(index + 1 >= count);
                if (ImGui::Button("v", ImVec2(width, 0.0f)))
                    command.moveDown = static_cast<int>(index);
                ImGui::EndDisabled();
                ImGui::SameLine(0.0f, 0.0f);
            }
            if (field.ops->eraseAt != nullptr && ImGui::Button("x", ImVec2(width, 0.0f)))
                command.remove = static_cast<int>(index);
        }

        // A std::vector. Composite elements get a tree node with the controls
        // right-aligned on its row; scalar elements share one property table
        // with the controls in the value cell. Both layouts follow what
        // drawPostProcessStack established by hand.
        bool drawDynArray(const Ecs::FieldDescriptor &field, void *value, const char *label,
                          const Utility::ComponentDrawContext &context, const FieldHook &hook)
        {
            const Ecs::FieldOps *ops = field.ops;
            const Ecs::FieldDescriptor *element = field.element;
            if (ops == nullptr || ops->size == nullptr || ops->elementAt == nullptr || element == nullptr)
                return false;

            const size_t count = ops->size(value);
            const bool open = ImGui::TreeNodeEx(label, ImGuiTreeNodeFlags_DefaultOpen, "%s (%zu)",
                                                label, count);
            if (!open)
                return false;

            bool changed = false;
            const bool readOnly = hasFlag(field.flags, Ecs::FieldFlags::ReadOnly);
            const bool controls = elementControlsEnabled(field, readOnly);
            const float controlsWidth = controls ? elementControlsWidth(field) : 0.0f;
            const bool composite = element->layout == Ecs::Layout::Struct ||
                                   element->layout == Ecs::Layout::DynArray;
            ElementCommand command;

            // Scalars share one table across all elements so their widgets line
            // up; composites each open their own tree.
            bool tableOpen = !composite && count > 0 && Widgets::beginProperties("##elements");

            for (size_t i = 0; i < count; ++i)
            {
                void *slot = ops->elementAt(value, i);
                if (slot == nullptr)
                    continue;

                ImGui::PushID(static_cast<int>(i));

                // A TitleFrom annotation labels the row from the element
                // itself; without one a collapsed row says only its index,
                // which is useless the moment there is more than one.
                char elementLabel[64];
                const char *title = field.ui.title != nullptr ? field.ui.title(slot) : nullptr;
                if (title != nullptr && *title != '\0')
                    std::snprintf(elementLabel, sizeof elementLabel, "%zu. %s", i + 1, title);
                else
                    std::snprintf(elementLabel, sizeof elementLabel, "%zu", i);

                // The element descriptor carries the element type's own layout,
                // semantic, nested and element pointers, so a vector<vector<T>>
                // or a vector<SomeStruct> recurses to whatever depth it has.
                if (composite)
                {
                    const float lineStartX = ImGui::GetCursorPosX();
                    const float available = ImGui::GetContentRegionAvail().x;

                    // AllowOverlap, or the full-width tree node owns the hover
                    // for those pixels and the buttons never register a click.
                    if (controls)
                        ImGui::SetNextItemAllowOverlap();

                    // "##element" is the id, elementLabel the visible text --
                    // the id must stay stable while the title changes, or
                    // renaming an effect collapses its own tree node.
                    const bool elementOpen = ImGui::TreeNodeEx(
                        "##element",
                        ImGuiTreeNodeFlags_DefaultOpen |
                            (controls ? ImGuiTreeNodeFlags_AllowOverlap : 0),
                        "%s", elementLabel);

                    if (controls)
                    {
                        ImGui::SameLine(lineStartX + available - controlsWidth);
                        drawElementControls(field, i, count, command);
                    }

                    if (elementOpen)
                    {
                        if (element->layout == Ecs::Layout::Struct && element->nested != nullptr)
                            changed |= drawFieldList(*element->nested, slot, context, hook);
                        else if (element->layout == Ecs::Layout::DynArray)
                            changed |= drawDynArray(*element, slot, elementLabel, context, hook);
                        ImGui::TreePop();
                    }
                }
                else if (tableOpen)
                {
                    const Utility::FieldDrawFn draw = fieldDrawers().resolve(*element);
                    if (draw != nullptr)
                    {
                        Widgets::propertyLabel(elementLabel, element->ui.tooltip);
                        if (readOnly)
                            ImGui::BeginDisabled();
                        // propertyLabel stretched the item to the whole cell;
                        // give the buttons their share back.
                        if (controls)
                            ImGui::SetNextItemWidth(
                                -(controlsWidth + ImGui::GetStyle().ItemSpacing.x));
                        const bool edited = draw({&context, element, slot});
                        if (readOnly)
                            ImGui::EndDisabled();
                        if (edited && !readOnly)
                        {
                            changed = true;
                            hook.fire(context);
                        }
                        if (controls)
                        {
                            ImGui::SameLine();
                            drawElementControls(field, i, count, command);
                        }
                    }
                }

                ImGui::PopID();
            }

            if (tableOpen)
                Widgets::endProperties();

            if (command.apply(*ops, value, count))
                changed = true;

            if (!readOnly && ops->resize != nullptr)
            {
                if (ImGui::SmallButton("Add"))
                {
                    ops->resize(value, count + 1);
                    changed = true;
                }
                if (ops->clear != nullptr && count > 0)
                {
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Clear"))
                    {
                        ops->clear(value);
                        changed = true;
                    }
                }
            }

            ImGui::TreePop();

            if (changed)
                hook.fire(context);
            return changed;
        }

        bool sameCategory(const char *a, const char *b)
        {
            if (a == nullptr || b == nullptr)
                return a == b;
            return std::strcmp(a, b) == 0;
        }

        // Draws the fields whose category matches `category` (null == the
        // ungrouped run), in declaration order. Scalars share one property
        // table; a struct or a vector closes it, draws its tree, and the next
        // scalar opens a fresh one -- ImGui tables cannot nest.
        bool drawCategory(const Ecs::TypeDescriptor &type, void *object, const char *category,
                          const Utility::ComponentDrawContext &context, FieldHook hook)
        {
            bool changed = false;
            bool tableOpen = false;
            int runIndex = 0;

            // Semantics whose editor is too big for a table cell. Queued as
            // their rows are drawn and flushed when the run's table closes, so
            // the expanded content appears directly beneath the rows it belongs
            // to rather than at the very end of the component.
            struct PendingExpansion
            {
                Utility::FieldDrawFn draw;
                const Ecs::FieldDescriptor *field;
                void *value;
            };
            std::vector<PendingExpansion> pending;

            // BeginTable pushes an id that EndTable pops, so the whole table
            // has to sit OUTSIDE the per-field PushID/PopID pair -- opening one
            // inside a field's scope and closing it after PopID unbalances the
            // id stack and trips ImGui's EndTable assert.
            //
            // Each run also needs its own id: a struct or vector field splits
            // the scalars into two tables, and reusing one id twice in a frame
            // within the same scope is an id collision.
            const auto openTable = [&] {
                if (tableOpen)
                    return;
                char id[32];
                std::snprintf(id, sizeof id, "##fields%d", runIndex++);
                tableOpen = Widgets::beginProperties(id);
            };
            const auto closeTable = [&] {
                if (tableOpen)
                {
                    Widgets::endProperties();
                    tableOpen = false;
                }

                for (const PendingExpansion &expansion : pending)
                {
                    ImGui::PushID(static_cast<int>(expansion.field->offset));
                    expansion.draw({&context, expansion.field, expansion.value});
                    ImGui::PopID();
                }
                pending.clear();
            };

            for (uint32_t i = 0; i < type.fieldCount; ++i)
            {
                const Ecs::FieldDescriptor &field = type.fields[i];
                if (hasFlag(field.flags, Ecs::FieldFlags::HideInInspector))
                    continue;
                if (!sameCategory(field.ui.category, category))
                    continue;
                // VisibleIf / HiddenIf. Evaluated against the owning struct, so
                // it can only ever read a sibling -- which is what makes it safe
                // to call here, mid-walk, with no risk of reentering the walker.
                if (field.ui.visibleWhen != nullptr && !field.ui.visibleWhen(object))
                    continue;

                void *value = static_cast<char *>(object) + field.offset;
                const char *label = displayName(field);
                const bool composite = field.layout == Ecs::Layout::Struct ||
                                       field.layout == Ecs::Layout::DynArray;

                // At the root the hook targets this field; deeper down it keeps
                // targeting whichever root field this subtree hangs off.
                FieldHook childHook = hook;
                if (childHook.field == nullptr)
                    childHook.field = &field;

                if (composite)
                    closeTable();
                else
                    openTable();

                if (!composite && !tableOpen)
                    continue;

                ImGui::PushID(static_cast<int>(field.offset));   // unique within a type

                if (field.layout == Ecs::Layout::Struct)
                    changed |= drawNestedStruct(field, value, label, context, childHook);
                else if (field.layout == Ecs::Layout::DynArray)
                    changed |= drawDynArray(field, value, label, context, childHook);
                else
                {
                    changed |= drawLeafRow(field, value, label, context, childHook);
                    if (const Utility::FieldDrawFn expand = fieldDrawers().resolveExpansion(field))
                        pending.push_back({expand, &field, value});
                }

                ImGui::PopID();
            }

            closeTable();
            return changed;
        }

        bool drawFieldList(const Ecs::TypeDescriptor &type, void *object,
                           const Utility::ComponentDrawContext &context, FieldHook hook)
        {
            if (type.fields == nullptr)
                return false;

            // Ungrouped fields first, at the top level of the card.
            bool changed = drawCategory(type, object, nullptr, context, hook);

            // Then one sub-section per category, in the order the categories
            // are first declared -- declaration order stays the thing the
            // component author controls, with no sorting surprises.
            std::vector<const char *> seen;
            for (uint32_t i = 0; i < type.fieldCount; ++i)
            {
                const char *category = type.fields[i].ui.category;
                if (category == nullptr || hasFlag(type.fields[i].flags, Ecs::FieldFlags::HideInInspector))
                    continue;

                bool already = false;
                for (const char *previous : seen)
                    already = already || sameCategory(previous, category);
                if (already)
                    continue;
                seen.push_back(category);

                // subSection is a bare CollapsingHeader and opens no id scope,
                // so without this every category's table run would reuse the
                // ungrouped run's ids.
                ImGui::PushID(category);
                if (Widgets::subSection(category))
                    changed |= drawCategory(type, object, category, context, hook);
                ImGui::PopID();
            }

            return changed;
        }
    }

    bool drawFields(const Ecs::TypeDescriptor &type, void *object,
                    const Utility::ComponentDrawContext &context)
    {
        if (object == nullptr)
            return false;
        return drawFieldList(type, object, context, FieldHook{&type, object, nullptr});
    }
}
