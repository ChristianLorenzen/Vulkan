#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "Core/ECS/Reflection/TypeDescriptor.hpp"
#include "Editor/Utility/ComponentDrawRegistry.hpp"

namespace Faye::Editor::Utility
{
    // Everything a field drawer is allowed to see. A struct rather than three
    // parameters because this will grow (undo scope, drag-and-drop payload)
    // and FieldDrawFn's signature should survive that.
    struct FieldDrawContext
    {
        // Asset registries, the owning entity, the shared texture picker.
        const ComponentDrawContext *owner = nullptr;
        // Name, flags, semantic id, layout, and the FieldUI annotations.
        const Ecs::FieldDescriptor *field = nullptr;
        // Already offset: (char *)component + field->offset, or the address of
        // a container element. A drawer never does pointer arithmetic itself.
        void *value = nullptr;
    };

    // Returns true iff the value was edited THIS FRAME. The return value is
    // what drives TypeDescriptor::onFieldChanged, so a drawer that mutates
    // without reporting will silently skip invariant repair.
    using FieldDrawFn = bool (*)(const FieldDrawContext &);

    // The editor-side mirror of Ecs::SemanticRegistry: same two keys, same
    // precedence, different output. Core turns a field into bytes; this turns
    // one into pixels. Kept separate because SemanticOps lives in headless
    // Core/ECS/Reflection and its signatures cannot carry ImGui state.
    class FieldDrawRegistry
    {
    public:
        // A semantic drawer knows what the field MEANS: faye.ModelAsset draws
        // an asset combo, not the uint32 it happens to be. Only register one
        // where Layout genuinely cannot express the widget -- "faye.f32" needs
        // no entry, Layout::F32 already draws it.
        void registerSemantic(Ecs::TypeId id, FieldDrawFn draw);

        // A layout drawer is the fallback: it knows only what bytes are there.
        void registerLayout(Ecs::Layout layout, FieldDrawFn draw);

        // A second, optional drawer for a semantic that needs more than a table
        // cell. A MaterialHandle is the motivating case: the row is a combo,
        // but the assigned material's full editor -- name, shader, property
        // grid, texture slots -- draws its own tables and sections, and ImGui
        // tables do not nest. The walker runs these after the property table
        // closes, so the expanded content lands directly beneath its rows.
        //
        // Semantic-only by design: a Layout is a byte pattern and can never
        // justify a whole sub-editor.
        void registerSemanticExpansion(Ecs::TypeId id, FieldDrawFn draw);

        // Semantic first, then layout, then null. The precedence lives here so
        // that every call site gets it right by construction -- it is the same
        // order SemanticRegistry uses, and the two must not drift.
        FieldDrawFn resolve(const Ecs::FieldDescriptor &field) const;

        // The expansion for this field, or null. Never falls back to layout.
        FieldDrawFn resolveExpansion(const Ecs::FieldDescriptor &field) const;

        // No option-provider table: FieldUI::options IS the provider, spliced
        // out of the annotation at describe time. Nothing to register and
        // nothing to look up.

    private:
        // Layout is a closed enum the engine owns, so a dense array beats a
        // hash lookup. Semantic ids are sparse 64-bit FNV hashes, so those get
        // a map. kLayoutCount must track Ecs::Layout; the static_assert in the
        // .cpp catches a forgotten enumerator.
        static constexpr size_t kLayoutCount = static_cast<size_t>(Ecs::Layout::Opaque) + 1;

        std::array<FieldDrawFn, kLayoutCount> layouts{};
        std::unordered_map<Ecs::TypeId, FieldDrawFn> semantics;
        std::unordered_map<Ecs::TypeId, FieldDrawFn> expansions;
    };
}
