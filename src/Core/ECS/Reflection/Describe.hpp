#pragma once

// The ONLY file in the engine that includes <meta>. Everything downstream sees
// plain TypeDescriptor data, so a GCC 16 reflection bug is a local problem.
//
// Verified against g++-16 (trunk r16-8100) on 2026-08-04. What that compiler
// actually supports, and the three places it bites:
//
//   1. std::define_static_string / std::define_static_array live in `std`,
//      NOT `std::meta` (P3491 is a separate paper from P2996).
//   2. A splice used as a template argument must be written
//      `typename [: X :]`; a bare `[: X :]` is rejected.
//   3. A std::meta::info FUNCTION PARAMETER is never a constant expression,
//      even inside a consteval function -- so any helper that splices or
//      extracts a deduced type must take the reflection as a TEMPLATE
//      parameter. Helpers that only compare reflections may take it normally.
//
// See Annotations.hpp for the fourth: annotation values may not contain
// `const char *` members.

#include "TypeDescriptor.hpp"

#if defined(FAYE_HAS_REFLECTION) && FAYE_HAS_REFLECTION

#include <array>
#include <cstddef>
#include <meta>
#include <new>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

namespace Faye::Ecs
{
    namespace Describe_
    {
        namespace sm = std::meta;

        template <class>          inline constexpr bool isVector = false;
        template <class T, class A> inline constexpr bool isVector<std::vector<T, A>> = true;

        template <class T> struct VectorElement { using type = void; };
        template <class T, class A> struct VectorElement<std::vector<T, A>> { using type = T; };

        // std::vector<bool> is a bitfield proxy: elementAt cannot return a void*
        // into it, so it is deliberately Opaque rather than silently broken.
        template <class T>
        inline constexpr bool isWalkableVector =
            isVector<T> && !std::is_same_v<typename VectorElement<T>::type, bool>;

        // A type we are willing to recurse into: public members only, no
        // invariants to violate. Uuid (private members, constructors) fails
        // this and stays Opaque, which is correct -- it has a semantic instead.
        //
        // A declared semantic makes a type a LEAF even when it is an aggregate.
        // ModelHandle is `struct { uint32_t value; }`, so without this rule the
        // descriptor would walk into it and emit a `value: 1` integer field
        // instead of letting SemanticRegistry write an asset id.
        template <class T>
        inline constexpr bool isDescribableStruct =
            std::is_class_v<T> && std::is_aggregate_v<T> && !isVector<T> &&
            !std::is_same_v<T, std::string> && semanticIdOf<T> == 0;

        // ------------------------------------------------------------------
        // Layout: closed BY DESIGN. Layout enumerates machine representations,
        // a fixed set the engine owns. New user types do not extend it; they
        // get Opaque plus a semantic TypeId (see SemanticTraits).
        // ------------------------------------------------------------------
        template <class T>
        consteval Layout layoutOf()
        {
            using U = std::remove_cv_t<T>;

            if constexpr (std::is_same_v<U, bool>)              return Layout::Bool;
            else if constexpr (std::is_same_v<U, std::string>)  return Layout::String;
            else if constexpr (std::is_enum_v<U>)               return layoutOf<std::underlying_type_t<U>>();
            else if constexpr (std::is_floating_point_v<U>)     return sizeof(U) == 4 ? Layout::F32 : Layout::F64;
            else if constexpr (std::is_integral_v<U>)
            {
                if constexpr (std::is_signed_v<U>)
                    return sizeof(U) == 1 ? Layout::I8 : sizeof(U) == 2 ? Layout::I16
                         : sizeof(U) == 4 ? Layout::I32 : Layout::I64;
                else
                    return sizeof(U) == 1 ? Layout::U8 : sizeof(U) == 2 ? Layout::U16
                         : sizeof(U) == 4 ? Layout::U32 : Layout::U64;
            }
            else if constexpr (std::is_same_v<U, glm::vec2>)    return Layout::F32x2;
            else if constexpr (std::is_same_v<U, glm::vec3>)    return Layout::F32x3;
            else if constexpr (std::is_same_v<U, glm::vec4>)    return Layout::F32x4;
            else if constexpr (std::is_same_v<U, glm::mat3>)    return Layout::F32x3x3;
            else if constexpr (std::is_same_v<U, glm::mat4>)    return Layout::F32x4x4;
            else if constexpr (isWalkableVector<U>)             return Layout::DynArray;
            else if constexpr (isDescribableStruct<U>)          return Layout::Struct;
            else                                               return Layout::Opaque;
        }

        // ------------------------------------------------------------------
        // Container access thunks.
        // ------------------------------------------------------------------
        template <class V>
        inline constexpr FieldOps kVectorOps{
            +[](const void *p) -> size_t { return static_cast<const V *>(p)->size(); },
            +[](void *p, size_t i) -> void * { return static_cast<void *>(&(*static_cast<V *>(p))[i]); },
            +[](void *p, size_t n) { static_cast<V *>(p)->resize(n); },
            +[](void *p) { static_cast<V *>(p)->clear(); },
            // Bounds-checked: the editor decides on an index while drawing and
            // applies it after the loop, by which point the container may have
            // been changed by something else in the same frame.
            +[](void *p, size_t i) {
                V *v = static_cast<V *>(p);
                if (i < v->size())
                    v->erase(v->begin() + static_cast<typename V::difference_type>(i));
            },
            +[](void *p, size_t a, size_t b) {
                V *v = static_cast<V *>(p);
                if (a < v->size() && b < v->size() && a != b)
                    std::swap((*v)[a], (*v)[b]);
            },
        };

        inline constexpr FieldOps kStringOps{
            +[](const void *p) -> size_t { return static_cast<const std::string *>(p)->size(); },
            +[](void *p, size_t i) -> void * { return static_cast<void *>(&(*static_cast<std::string *>(p))[i]); },
            +[](void *p, size_t n) { static_cast<std::string *>(p)->resize(n); },
            +[](void *p) { static_cast<std::string *>(p)->clear(); },
        };

        template <class T>
        consteval const FieldOps *opsFor()
        {
            using U = std::remove_cv_t<T>;
            if constexpr (isWalkableVector<U>)                 return &kVectorOps<U>;
            else if constexpr (std::is_same_v<U, std::string>) return &kStringOps;
            else                                              return nullptr;
        }

        // ------------------------------------------------------------------
        // Annotation readers.
        //
        // Text annotations are class TEMPLATES (Annotations.hpp explains why),
        // so they are matched on template_of rather than on exact type. The
        // reflection is a template parameter because the body both splices and
        // extracts a type deduced from it -- gotcha 3 above.
        // ------------------------------------------------------------------
        template <template <std::size_t> class Annotation, sm::info Member>
        consteval const char *annotationText()
        {
            template for (constexpr auto a : std::define_static_array(sm::annotations_of(Member)))
            {
                constexpr auto type = sm::dealias(sm::type_of(a));
                if constexpr (sm::has_template_arguments(type) && sm::template_of(type) == ^^Annotation)
                {
                    constexpr auto value = sm::extract<typename [: type :]>(a);
                    return std::define_static_string(std::string_view{value.text});
                }
            }
            return nullptr;
        }

        // ------------------------------------------------------------------
        // Enum dropdowns.
        //
        // No annotation: an enum's legal values ARE its enumerators, so the
        // type alone determines the widget and a per-field opt-in would just be
        // noise. An OptionsFrom annotation still overrides this (see
        // fieldArrayOf), which is the "type chooses, annotation refines" rule
        // the rest of this file follows.
        //
        // The names have to be promoted into a static array FIRST. Doing it
        // inside the provider lambda makes the lambda itself immediate (P2564
        // escalation, because define_static_string is consteval) and its
        // address is then unusable as a runtime function pointer -- GCC
        // reports "immediate evaluation returns address of immediate
        // function". Hoisting keeps the lambda an ordinary function.
        // ------------------------------------------------------------------
        template <class E>
        consteval size_t enumCountOf()
        {
            return sm::enumerators_of(^^E).size();
        }

        template <class E, size_t N>
        consteval std::array<EnumEntry, N> enumEntriesOf()
        {
            std::array<EnumEntry, N> entries{};
            size_t next = 0;
            template for (constexpr auto e : std::define_static_array(sm::enumerators_of(^^E)))
            {
                entries[next++] = EnumEntry{std::define_static_string(sm::identifier_of(e)),
                                            static_cast<int64_t>([: e :])};
            }
            return entries;
        }

        // Same static-storage rule as kFields: the table has to exist as an
        // object so the descriptor can point at it.
        template <class E>
        inline constexpr std::array<EnumEntry, enumCountOf<E>()> kEnumEntries =
            enumEntriesOf<E, enumCountOf<E>()>();

        template <class E>
        inline constexpr EnumOps kEnumOps{
            kEnumEntries<E>.data(),
            static_cast<uint32_t>(kEnumEntries<E>.size()),
            +[](const void *p) -> int64_t { return static_cast<int64_t>(*static_cast<const E *>(p)); },
            +[](void *p, int64_t v) { *static_cast<E *>(p) = static_cast<E>(v); },
        };

        template <class T>
        consteval const EnumOps *enumOpsFor()
        {
            using U = std::remove_cv_t<T>;
            if constexpr (std::is_enum_v<U>)
                return &kEnumOps<U>;
            else
                return nullptr;
        }

        // ------------------------------------------------------------------
        // Member-reflection annotations.
        //
        // parent_of gives the struct the member belongs to, which is what lets
        // these take a plain `const void *` and still be type-safe: the cast
        // target is derived from the annotation, not asserted by the caller.
        // ------------------------------------------------------------------
        template <sm::info Member>
        consteval TitleProviderFn titleProviderFrom()
        {
            using Owner = typename [: sm::parent_of(Member) :];
            return +[](const void *element) -> const char * {
                return static_cast<const Owner *>(element)->[: Member :].c_str();
            };
        }

        template <sm::info Member, bool WhenTrue>
        consteval VisibilityFn visibilityFrom()
        {
            using Owner = typename [: sm::parent_of(Member) :];
            return +[](const void *owner) -> bool {
                const bool flag = static_cast<const Owner *>(owner)->[: Member :];
                return WhenTrue ? flag : !flag;
            };
        }

        template <sm::info Member>
        consteval TitleProviderFn titleProviderOf()
        {
            constexpr auto matches =
                std::define_static_array(sm::annotations_of_with_type(Member, ^^TitleFrom));
            if constexpr (matches.size() == 0)
                return nullptr;
            else
                return titleProviderFrom<sm::extract<TitleFrom>(matches[0]).member>();
        }

        template <sm::info Member>
        consteval VisibilityFn visibilityOf()
        {
            constexpr auto shown =
                std::define_static_array(sm::annotations_of_with_type(Member, ^^VisibleIf));
            constexpr auto hidden =
                std::define_static_array(sm::annotations_of_with_type(Member, ^^HiddenIf));

            static_assert(shown.size() + hidden.size() <= 1,
                          "a field may carry at most one of VisibleIf / HiddenIf; two "
                          "conditions on one field have no defined combination yet");

            if constexpr (shown.size() == 1)
                return visibilityFrom<sm::extract<VisibleIf>(shown[0]).member, true>();
            else if constexpr (hidden.size() == 1)
                return visibilityFrom<sm::extract<HiddenIf>(hidden[0]).member, false>();
            else
                return nullptr;
        }

        // Splices an OptionsFrom annotation back into a callable function
        // pointer, so the descriptor carries the provider itself rather than a
        // name for the editor to look up.
        //
        // annotations_of_with_type rather than the template-for + template_of
        // pattern used above: that pattern is for the CTAD text annotations,
        // where the annotation's type varies with the string length. OptionsFrom
        // is a single concrete type, and matching it by type directly is both
        // simpler and -- verified on g++-16 -- the form that actually works.
        template <sm::info Member>
        consteval OptionProviderFn optionProviderOf()
        {
            constexpr auto matches =
                std::define_static_array(sm::annotations_of_with_type(Member, ^^OptionsFrom));
            if constexpr (matches.size() == 0)
            {
                return nullptr;
            }
            else
            {
                constexpr auto value = sm::extract<OptionsFrom>(matches[0]);
                // Address-of the spliced function: the splice names the
                // function itself, not a pointer to it.
                return &[: value.provider :];
            }
        }

        // No splice, no deduced type -- an ordinary parameter is fine here.
        consteval bool hasTag(sm::info member, sm::info tagType)
        {
            return !sm::annotations_of_with_type(member, tagType).empty();
        }

        consteval bool findRange(sm::info member, Range &out)
        {
            for (auto a : sm::annotations_of_with_type(member, ^^Range))
            {
                out = sm::extract<Range>(a);
                return true;
            }
            return false;
        }

        consteval FieldFlags flagsOf(sm::info member)
        {
            FieldFlags flags = FieldFlags::None;
            if (hasTag(member, ^^NotSerializedTag))   flags |= FieldFlags::NotSerialized;
            if (hasTag(member, ^^HideInInspectorTag)) flags |= FieldFlags::HideInInspector;
            if (hasTag(member, ^^ReadOnlyTag))        flags |= FieldFlags::ReadOnly;
            return flags;
        }

        template <class T> consteval TypeDescriptor describeImpl();
        template <class T> consteval FieldDescriptor typeFieldOf();
        template <class T> consteval size_t fieldCountOf();
        template <class T, size_t N> consteval std::array<FieldDescriptor, N> fieldArrayOf();

        // ------------------------------------------------------------------
        // Storage for a type's field array.
        //
        // NOT std::define_static_array, which is what this originally used.
        // On g++-16 (trunk r16-8100) promoting a FieldDescriptor array through
        // reflect_constant_array fails -- "is not a constant expression" -- once
        // ANY other descriptor has been instantiated earlier in the same
        // translation unit. The identical descriptor compiles cleanly on its
        // own, so the failure depends on unrelated preceding constant
        // evaluations rather than on anything about the type.
        //
        // A per-T constexpr std::array is the documented fallback
        // (REFLECTION_STEPS_1_1_TO_1_6.md 1.3b) and sidesteps the promotion
        // entirely: the array already has static storage, so .data() is a
        // stable pointer with no reflect_constant_array involved.
        // define_static_string, used for the names, is unaffected.
        // ------------------------------------------------------------------
        template <class T>
        inline constexpr std::array<FieldDescriptor, fieldCountOf<T>()> kFields =
            fieldArrayOf<T, fieldCountOf<T>()>();
    }

    // Published descriptor. One per T, static storage, address-stable: nested
    // fields point at it across translation units.
    template <class T>
    inline constexpr TypeDescriptor kDescriptor = Describe_::describeImpl<T>();

    template <class T>
    consteval const TypeDescriptor &describe() { return kDescriptor<T>; }

    namespace Describe_
    {
        // Descriptor for a container's element type. Static storage, so
        // FieldDescriptor::element can point at it across translation units.
        // Recursive by construction: vector<vector<float>> gets an element
        // descriptor that is itself a DynArray with its own element.
        template <class T>
        inline constexpr FieldDescriptor kElementDescriptor = typeFieldOf<T>();

        template <class T> consteval const TypeDescriptor *nestedFor();
        template <class T> consteval const FieldDescriptor *elementFor();

        // Only recurse where there is something to recurse into. Guarding on
        // the layout (not on is_class_v) keeps this in lockstep with layoutOf,
        // so nested can never disagree with Layout::Struct.
        template <class T>
        consteval const TypeDescriptor *nestedFor()
        {
            using U = std::remove_cv_t<T>;
            if constexpr (layoutOf<U>() == Layout::Struct)
                return &kDescriptor<U>;
            else
                return nullptr;
        }

        template <class T>
        consteval const FieldDescriptor *elementFor()
        {
            using U = std::remove_cv_t<T>;
            if constexpr (isWalkableVector<U>)
                return &kElementDescriptor<typename VectorElement<U>::type>;
            else
                return nullptr;
        }

        // The type-derived half of a FieldDescriptor -- everything knowable
        // from T alone. describeImpl() layers the member-derived half (name,
        // offset, flags, annotations) on top, and kElementDescriptor uses it
        // bare. One definition, so a field and a container element can never
        // disagree about what a type is.
        template <class T>
        consteval FieldDescriptor typeFieldOf()
        {
            using U = std::remove_cv_t<T>;

            static_assert(!isVector<U> || isWalkableVector<U>,
                          "std::vector<bool> is a bit-proxy container: its elements have no "
                          "address, so FieldOps::elementAt cannot reach them. Use "
                          "std::vector<uint8_t> for a serialisable flag array.");

            FieldDescriptor f{};
            f.typeId    = semanticIdOf<U>;
            f.layout    = layoutOf<U>();
            f.enumOps   = enumOpsFor<U>();
            f.size      = static_cast<uint32_t>(sizeof(U));
            f.elemCount = isWalkableVector<U> ? 0u : 1u;
            f.nested    = nestedFor<U>();
            f.element   = elementFor<U>();
            f.ops       = opsFor<U>();
            return f;
        }

        template <class T>
        consteval void (*defaultCtorFor())(void *)
        {
            if constexpr (std::is_default_constructible_v<T>)
                return +[](void *p) { ::new (p) T{}; };
            else
                return nullptr;
        }

        template <class T>
        consteval void (*onFieldChangedFor())(void *, const FieldDescriptor &, World &, Entity)
        {
            return FieldChangedTraits<T>::handler;
        }

        template <class T>
        consteval size_t fieldCountOf()
        {
            return sm::nonstatic_data_members_of(^^T, sm::access_context::current()).size();
        }

        template <class T, size_t N>
        consteval std::array<FieldDescriptor, N> fieldArrayOf()
        {
            std::array<FieldDescriptor, N> fields{};
            size_t next = 0;

            template for (constexpr auto m : std::define_static_array(
                              sm::nonstatic_data_members_of(^^T, sm::access_context::current())))
            {
                using Field = typename [: sm::type_of(m) :];

                // Everything derivable from the type, then the member-derived
                // half on top.
                FieldDescriptor f = typeFieldOf<Field>();

                // identifier_of returns a string_view that does not survive
                // escaping constant evaluation -- always promote it.
                f.name         = std::define_static_string(sm::identifier_of(m));
                f.flags        = flagsOf(m);
                f.offset       = static_cast<uint32_t>(sm::offset_of(m).bytes);
                f.size         = static_cast<uint32_t>(sm::size_of(m));
                f.serializedAs = annotationText<SerializedAs, m>();

                f.ui.tooltip     = annotationText<Tooltip, m>();
                f.ui.units       = annotationText<Units, m>();
                f.ui.category    = annotationText<Category, m>();
                f.ui.title       = titleProviderOf<m>();
                f.ui.visibleWhen = visibilityOf<m>();
                f.ui.colorPicker = hasTag(m, ^^ColorPickerTag);
                f.ui.radians     = hasTag(m, ^^RadiansTag);
                f.ui.orderable   = hasTag(m, ^^OrderableTag);

                f.ui.options     = optionProviderOf<m>();

                Range range{};
                if (findRange(m, range))
                {
                    f.ui.hasRange = true;
                    f.ui.rangeMin = range.min;
                    f.ui.rangeMax = range.max;
                }

                fields[next++] = f;
            }
            return fields;
        }

        template <class T>
        consteval TypeDescriptor describeImpl()
        {
            static_assert(std::is_class_v<T>, "describe<T> requires a class type");

            // TypeName wins over identifier_of so a C++ rename never touches
            // the persisted name.
            const char *name = annotationText<TypeName, ^^T>();
            if (name == nullptr)
                name = std::define_static_string(sm::identifier_of(^^T));

            TypeDescriptor d{};
            d.id               = hashTypeName(name);
            d.name             = name;
            d.fields           = kFields<T>.data();
            d.fieldCount       = static_cast<uint32_t>(kFields<T>.size());
            d.size             = static_cast<uint32_t>(sizeof(T));
            d.alignment        = static_cast<uint32_t>(alignof(T));
            d.constructDefault = defaultCtorFor<T>();
            d.onFieldChanged   = onFieldChangedFor<T>();
            return d;
        }
    }
}

#endif   // FAYE_HAS_REFLECTION
