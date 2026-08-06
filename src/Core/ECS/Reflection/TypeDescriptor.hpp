#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>

#include <glm/fwd.hpp>

#include "Annotations.hpp"

namespace Faye
{
    class Uuid;
    struct ModelHandle;
    struct MaterialHandle;
}

namespace Faye::Ecs
{
    class Serializer;
    class Deserializer;
    class World;
    struct Entity;

    struct TypeDescriptor;   // forward -- FieldDescriptor points at it

    using TypeId = uint64_t;

    // Compile-time FNV-1a. Stable across runs, machines and builds, which is the
    // whole point: ComponentId is first-use-ordered and must never be persisted.
    constexpr TypeId hashTypeName(const char *s)
    {
        TypeId h = 1469598103934665603ull;
        for (; *s; ++s) { h ^= static_cast<TypeId>(*s); h *= 1099511628211ull; }
        return h;
    }

    static_assert(sizeof(TypeId) == 8);
    static_assert(hashTypeName("glm.vec3") != hashTypeName("glm.vec4"));

    // ---------------------------------------------------------------------
    // Semantic identity: the extension point.
    //
    // A TypeId answers "what does this field MEAN", as opposed to Layout's
    // "what bytes live here". Deliberately an open trait rather than a closed
    // if-constexpr chain in Describe.hpp: a new asset or value type opts in
    // with one FAYE_SEMANTIC line next to its own definition, and no central
    // file changes. Unspecialised types get id 0, meaning "no semantic --
    // fall back to Layout".
    //
    // Lives here rather than in Describe.hpp so that declaring a semantic does
    // not drag <meta> into the declaring header. Describe.hpp is still the only
    // file in the engine that includes <meta>.
    // ---------------------------------------------------------------------
    template <class T>
    struct SemanticTraits
    {
        static constexpr TypeId id = 0;
    };

    template <class T>
    inline constexpr TypeId semanticIdOf = SemanticTraits<std::remove_cv_t<T>>::id;

    // WHAT BYTES LIVE AT THE OFFSET. Never semantics -- see IMPLEMENTATION_PLAN.md 3.
    enum class Layout : uint8_t {
        Bool, I8, I16, I32, I64, U8, U16, U32, U64, F32, F64,
        F32x2, F32x3, F32x4, F32x3x3, F32x4x4,
        Struct, DynArray, String, Opaque
    };

    // Scoped: the unscoped form collided with the identically named annotation
    // tag objects in Annotations.hpp (Ecs::NotSerialized et al), which live in
    // this same namespace. Still uint8_t-backed, so FieldDescriptor's layout is
    // unchanged.
    enum class FieldFlags : uint8_t {
        None            = 0,
        NotSerialized   = 1 << 0,
        HideInInspector = 1 << 1,
        ReadOnly        = 1 << 2,
    };

    constexpr FieldFlags operator|(FieldFlags a, FieldFlags b)
    {
        return static_cast<FieldFlags>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
    }
    constexpr FieldFlags &operator|=(FieldFlags &a, FieldFlags b) { return a = a | b; }
    constexpr bool hasFlag(FieldFlags value, FieldFlags flag)
    {
        return (static_cast<uint8_t>(value) & static_cast<uint8_t>(flag)) != 0;
    }

    // ---------------------------------------------------------------------
    // Dropdown options.
    //
    // A push-style sink rather than a returned std::vector: FieldDescriptor is
    // plain data that has to survive crossing a .so boundary, and a container
    // in that signature would make the descriptor ABI-fragile for the same
    // reason `fields` is a pointer and not a std::span.
    // ---------------------------------------------------------------------
    struct FieldOption {
        const char *label = nullptr;   // shown to the user; null means use value
        // String-valued fields carry `value`; enum and integer fields carry
        // `ordinal`. Genuinely two kinds of option, so the widget picks the one
        // matching the field's layout rather than converting between them.
        const char *value = nullptr;
        int64_t     ordinal = 0;
    };

    struct OptionSink {
        void *context = nullptr;
        void (*append)(void *context, FieldOption option) = nullptr;

        // For string-valued fields.
        void add(const char *value, const char *label = nullptr) const
        {
            if (append != nullptr)
                append(context, FieldOption{label, value, 0});
        }

        // For enum and integer fields.
        void addValue(int64_t ordinal, const char *label) const
        {
            if (append != nullptr)
                append(context, FieldOption{label, nullptr, ordinal});
        }
    };

    // ---------------------------------------------------------------------
    // Enums.
    //
    // On FieldDescriptor rather than FieldUI, because this decides BYTES ON
    // DISK: an enum is persisted by NAME, so a save survives someone inserting
    // an enumerator in the middle. That is already the convention here --
    // MaterialEnumNames.cpp hand-writes exactly this table for the material
    // enums -- and generating it is what lets that file eventually go away.
    //
    // read/write rather than a raw offset because the underlying type varies
    // (uint8_t through int64_t) and generic code cannot reinterpret a pointer
    // without knowing the width.
    // ---------------------------------------------------------------------
    struct EnumEntry {
        const char *name = nullptr;
        int64_t     value = 0;
    };

    struct EnumOps {
        const EnumEntry *entries = nullptr;
        uint32_t         count = 0;
        int64_t (*read)(const void *) = nullptr;
        void (*write)(void *, int64_t) = nullptr;

        // Null when the stored value matches no enumerator -- a save written by
        // a newer build, or a value cast in from an int.
        const char *nameOf(int64_t value) const
        {
            for (uint32_t i = 0; i < count; ++i)
                if (entries[i].value == value)
                    return entries[i].name;
            return nullptr;
        }

        bool valueOf(const char *name, int64_t &out) const
        {
            if (name == nullptr)
                return false;
            for (uint32_t i = 0; i < count; ++i)
            {
                const char *candidate = entries[i].name;
                const char *lhs = candidate;
                const char *rhs = name;
                while (*lhs != '\0' && *lhs == *rhs) { ++lhs; ++rhs; }
                if (*lhs == '\0' && *rhs == '\0')
                {
                    out = entries[i].value;
                    return true;
                }
            }
            return false;
        }
    };

    // Labels one row of a container from the element itself. Returns a pointer
    // owned by the element, valid until the element changes.
    using TitleProviderFn = const char *(*)(const void *element);

    // Whether a field should be drawn at all, given the struct that owns it.
    // Null means "always".
    using VisibilityFn = bool (*)(const void *owner);

    // Called every time the dropdown is drawn, which is the point: the option
    // set is whatever the runtime registry says right now, so a hot-reloaded
    // library shows up without the editor knowing it exists. Providers hand out
    // pointers they own -- the sink copies nothing.
    using OptionProviderFn = void (*)(const OptionSink &);

    // Editor-only hints. Never affects bytes on disk.
    struct FieldUI {
        float       rangeMin = 0.0f;
        float       rangeMax = 0.0f;
        bool        hasRange = false;
        bool        radians = false;      // stored radians, drawn degrees
        bool        colorPicker = false;
        // Container fields only: offer reordering controls per element.
        bool        orderable = false;
        const char *tooltip = nullptr;
        const char *units = nullptr;

        // Supplies the dropdown entries; null means "no dropdown, draw the
        // field's normal widget". Spliced straight out of the OptionsFrom
        // annotation at describe time, so there is no name to resolve and no
        // registry to consult -- a provider that no longer exists is a compile
        // error at the annotation, not a silently plain text box at runtime.
        OptionProviderFn options = nullptr;

        // Container fields only: labels each row from the element. Null falls
        // back to the index.
        TitleProviderFn title = nullptr;

        // Null means always visible. Evaluated against the OWNING struct, so a
        // condition can only ever name a sibling -- which is the whole reason
        // it is safe to evaluate while walking.
        VisibilityFn visibleWhen = nullptr;

        // Inspector grouping. Null means "no group": the field is drawn at the
        // top level of the component card, before any grouped field. Fields
        // sharing a category are collected under one collapsible sub-section,
        // in the order the categories are first seen in the field list --
        // declaration order stays the thing the author controls.
        const char *category = nullptr;
    };

    // Required for DynArray / String / Opaque. Without these, generic code
    // cannot walk a std::vector or std::string at all.
    struct FieldOps {
        size_t (*size)(const void *) = nullptr;
        void *(*elementAt)(void *, size_t) = nullptr;
        void (*resize)(void *, size_t) = nullptr;
        void (*clear)(void *) = nullptr;

        // Enough to edit a container, not just walk it. resize/clear alone can
        // only append and drop from the end, which makes "remove this element"
        // and "move it up" impossible to express generically -- the editor
        // would need a hand-written drawer for every ordered list.
        //
        // Both are no-ops on an out-of-range index rather than UB: the editor
        // computes indices a frame before it applies them.
        void (*eraseAt)(void *, size_t) = nullptr;
        void (*swapElements)(void *, size_t, size_t) = nullptr;
    };

    struct FieldDescriptor {
        const char           *name = nullptr;
        TypeId                typeId = 0;        // SEMANTIC identity
        Layout                layout = Layout::Opaque;
        FieldFlags            flags = FieldFlags::None;
        uint32_t              offset = 0;
        uint32_t              size = 0;
        uint32_t              elemCount = 1;     // 1 scalar - N fixed array - 0 dynamic

        // Set iff layout == Struct.
        const TypeDescriptor *nested = nullptr;

        // Set iff layout == DynArray. Describes the ELEMENT type, and carries
        // its own layout / typeId / nested / ops -- so a container element can
        // be anything a field can be, including another container.
        //
        // A whole FieldDescriptor rather than a few flat elemLayout/elemTypeId
        // members because an element needs every axis a field needs: a
        // vector<string> element needs `ops`, a vector<Inner> element needs
        // `nested`, a vector<vector<float>> element needs `element`. `name`,
        // `offset`, `flags` and `ui` are meaningless here and stay zeroed --
        // elements are addressed through ops->elementAt, never by offset.
        const FieldDescriptor *element = nullptr;

        const FieldOps       *ops = nullptr;

        // Set iff the field is an enum. Its layout stays the underlying integer
        // -- this is what the value MEANS, not how it is stored.
        const EnumOps        *enumOps = nullptr;
        const char           *serializedAs = nullptr;   // rename migration alias
        FieldUI               ui{};
    };

    // Signature of the "this field was just edited" hook. Named once so a
    // specialisation that gets it wrong is an error AT THE SPECIALISATION,
    // rather than a deep one inside describeImpl. (With `auto handler =
    // nullptr` the type is std::nullptr_t and nothing is checked.)
    using FieldChangedFn = void (*)(void *component, const FieldDescriptor &field,
                                    World &world, Entity entity);

    // Per-type invariant repair -- CameraComponent::primary must demote every
    // other camera, WaterComponent::subdivisions clamps to 4..256, and so on.
    // Describe.hpp is generic and cannot know those rules, so it reads them
    // from here: the same open-trait shape as SemanticTraits.
    //
    // MUST be specialised in a HEADER. kDescriptor<T> is an inline constexpr
    // variable, so every translation unit that instantiates it has to see the
    // same handler -- otherwise it is an ODR violation, and the compiler will
    // not tell you. Declare the handler in the header, define it in a .cpp that
    // can include World/Scene.
    template <class T>
    struct FieldChangedTraits
    {
        static constexpr FieldChangedFn handler = nullptr;
    };

    struct TypeDescriptor {
        uint32_t               structVersion = 1;   // validated at plugin load (D8)
        uint32_t               structSize = sizeof(TypeDescriptor);
        TypeId                 id = 0;
        const char            *name = nullptr;
        uint32_t               version = 1;         // per-type schema version
        const FieldDescriptor *fields = nullptr;    // NOT std::span - crosses .so
        uint32_t               fieldCount = 0;
        uint32_t               size = 0;
        uint32_t               alignment = 0;

        void (*constructDefault)(void *) = nullptr;
        void (*onFieldChanged)(void *, const FieldDescriptor &, World &, Entity) = nullptr;
        void (*migrate)(uint32_t fromVersion, Deserializer &, void *) = nullptr;
    };
}

// Declares the semantic identity of a type. Put it at GLOBAL scope in the
// header that defines the type -- that is the whole point, adding a semantic
// must not require editing a central list.
//
//     FAYE_SEMANTIC(Faye::TextureHandle, "faye.TextureAsset");
//
// Global scope specifically: the macro reopens Faye::Ecs, and GCC rejects the
// alternative spelling (`template <> struct ::Faye::Ecs::SemanticTraits<T>`)
// with "global qualification of class name is invalid".
//
// The name string is hashed, never stored, so it only has to be unique and
// stable -- it is what makes scene files survive a C++ type rename.
#define FAYE_SEMANTIC(Type, Name)                                              \
    namespace Faye::Ecs                                                        \
    {                                                                          \
        template <>                                                            \
        struct SemanticTraits<Type>                                            \
        {                                                                      \
            static constexpr TypeId id = hashTypeName(Name);                   \
        };                                                                     \
    }                                                                          \
    static_assert(::Faye::Ecs::semanticIdOf<Type> != 0,                        \
                  "semantic name hashed to 0, which means \"no semantic\"")

// Built-in semantics. These are declared here rather than beside their types
// because they are either standard-library types or engine primitives that
// every descriptor needs; anything new should follow the ModelHandle pattern
// and declare FAYE_SEMANTIC in its own header instead.
FAYE_SEMANTIC(bool,        "faye.bool");
FAYE_SEMANTIC(int32_t,     "faye.i32");
FAYE_SEMANTIC(uint32_t,    "faye.u32");
FAYE_SEMANTIC(float,       "faye.f32");
FAYE_SEMANTIC(std::string, "faye.string");
FAYE_SEMANTIC(glm::vec2,   "glm.vec2");
FAYE_SEMANTIC(glm::vec3,   "glm.vec3");
FAYE_SEMANTIC(glm::vec4,   "glm.vec4");

// Forward-declared above: a trait specialisation does not need a complete type,
// which keeps Boost (Uuid) and the asset registries out of this header.
FAYE_SEMANTIC(Faye::Uuid,           "faye.Uuid");
FAYE_SEMANTIC(Faye::ModelHandle,    "faye.ModelAsset");
FAYE_SEMANTIC(Faye::MaterialHandle, "faye.MaterialAsset");

// Attaches an invariant-repair hook to a type. Global scope, same rule as
// FAYE_SEMANTIC, and for the same reason.
//
//     void onCameraFieldChanged(void *, const Faye::Ecs::FieldDescriptor &,
//                               Faye::Ecs::World &, Faye::Ecs::Entity);
//     FAYE_ON_FIELD_CHANGED(Faye::CameraComponent, &onCameraFieldChanged);
#define FAYE_ON_FIELD_CHANGED(Type, Handler)                                   \
    namespace Faye::Ecs                                                        \
    {                                                                          \
        template <>                                                            \
        struct FieldChangedTraits<Type>                                        \
        {                                                                      \
            static constexpr FieldChangedFn handler = Handler;                 \
        };                                                                     \
    }                                                                          \
    static_assert(::Faye::Ecs::FieldChangedTraits<Type>::handler != nullptr,   \
                  "FAYE_ON_FIELD_CHANGED handler must not be null")
