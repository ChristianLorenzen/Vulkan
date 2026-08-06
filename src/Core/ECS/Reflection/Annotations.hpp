#pragma once

#include <cstddef>

namespace Faye::Ecs
{
    // GCC 16 cannot extract() an annotation value that holds a `const char *`
    // -- std::meta::extract throws "reflect_constant failed", and splicing the
    // annotation instead ([: a :]) is rejected outright ("cannot use an
    // annotation in a splice expression"). A fixed char array IS extractable,
    // so text annotations carry one and Describe.hpp promotes it back to a
    // `const char *` with std::define_static_string.
    //
    // The length is a deduced template parameter, so there is no maximum and
    // no padding: Units("m") is Units<2>. Describe.hpp finds these by comparing
    // std::meta::template_of against the primary template, not by exact type.

#define FAYE_TEXT_ANNOTATION(Name)                                             \
    template <std::size_t N>                                                   \
    struct Name                                                                \
    {                                                                          \
        char text[N]{};                                                        \
        consteval Name(const char (&source)[N])                                \
        {                                                                      \
            for (std::size_t i = 0; i < N; ++i) text[i] = source[i];           \
        }                                                                      \
    }

    FAYE_TEXT_ANNOTATION(Tooltip);       // hover text in the inspector
    FAYE_TEXT_ANNOTATION(Units);         // suffix drawn after the value ("m", "deg")
    FAYE_TEXT_ANNOTATION(SerializedAs);  // previous key name; read-only migration alias
    FAYE_TEXT_ANNOTATION(Category);      // inspector grouping
    FAYE_TEXT_ANNOTATION(TypeName);      // explicit persisted type name, overrides identifier_of

    // (Options used to be a text annotation naming a provider the editor bound
    // by string. See OptionsFrom below for why it is a reflection now.)

#undef FAYE_TEXT_ANNOTATION

    // Numeric annotation values need no such workaround -- extract() handles
    // them directly, so this stays a plain aggregate.
    struct Range { float min, max; };

#if defined(FAYE_HAS_REFLECTION) && FAYE_HAS_REFLECTION
    // Draws the field as a dropdown whose entries a function supplies at draw
    // time:
    //
    //     void postProcessEffectOptions(const Ecs::OptionSink &);
    //     FAYE_ATTR(Ecs::OptionsFrom{^^Faye::postProcessEffectOptions})
    //     std::string definitionId;
    //
    // A REFLECTION of the provider, not its name as a string. Both Unity
    // (Odin's ValueDropdown("MemberName")) and Unreal (meta=(GetOptions="Fn"))
    // use strings here, and both pay for it the same way: nothing checks the
    // name, a rename silently degrades the field to a plain text box, and the
    // failure only shows up when someone opens that panel. A reflection is
    // checked by the compiler and follows renames, which is the one place a
    // P2996 engine can straightforwardly beat both.
    //
    // Describe.hpp splices this back into a real function pointer, so the
    // descriptor carries no name to look up and the editor needs no registry.
    //
    // `decltype(^^int)` rather than std::meta::info: naming the type this way
    // needs -freflection but NOT <meta>, which is what keeps this header --
    // included by every component header in the engine -- out of the two-TU
    // budget Describe.hpp is deliberately confined to.
    using Info = decltype(^^int);

    struct OptionsFrom { Info provider; };

    // Labels a container's rows from a member of its ELEMENT type, instead of
    // by index. Unreal's meta=(TitleProperty="{Name}"), except the member is
    // named by reflection, so it is checked and follows renames:
    //
    //     FAYE_ATTR(Ecs::TitleFrom{^^Faye::PostProcessEffectComponent::definitionId})
    //     std::vector<PostProcessEffectComponent> effects;
    //
    // Goes on the CONTAINER field but names a member of the element -- a
    // collapsed row is only worth reading if it says which element it is.
    struct TitleFrom { Info member; };

    // Conditional visibility, keyed on a SIBLING bool in the same struct:
    //
    //     bool castsShadows = true;
    //     FAYE_ATTR(Ecs::VisibleIf{^^Light::castsShadows}) float shadowBias = 0.01f;
    //     FAYE_ATTR(Ecs::HiddenIf{^^Light::castsShadows})  float fakeAo = 0.0f;
    //
    // Deliberately the narrowest thing that is useful: one bool, shown or
    // hidden. Unreal grew this into an expression mini-language (`A > 10 &&
    // !B`) parsed from a string at draw time, and explicitly cannot call
    // functions. Because this stores a PREDICATE rather than a parsed string,
    // richer conditions are a change to what produces the predicate -- the
    // descriptor slot and the drawer do not move.
    struct VisibleIf { Info member; };
    struct HiddenIf  { Info member; };
#endif

    // Valueless tags -- each needs to be a distinct TYPE so annotations_of_with_type
    // can find it. Empty structs, instantiated as inline constexpr objects.
    struct ColorPickerTag     {};
    struct RadiansTag         {};
    struct NotSerializedTag   {};
    struct HideInInspectorTag {};
    struct ReadOnlyTag        {};
    struct OrderableTag       {};

    inline constexpr ColorPickerTag     ColorPicker{};
    inline constexpr RadiansTag         Radians{};
    inline constexpr NotSerializedTag   NotSerialized{};
    inline constexpr HideInInspectorTag HideInInspector{};
    inline constexpr ReadOnlyTag        ReadOnly{};

    // Per-FIELD, not per-element-type: whether reordering is meaningful is a
    // property of what the container means, not of what it holds. A stack of
    // post-process effects is order-dependent; a vector<float> of unrelated
    // coefficients beside it is not, even though both hold the same element
    // type somewhere in the engine.
    inline constexpr OrderableTag       Orderable{};
}

// Annotations are reflection syntax, so they must vanish when -freflection is off.
// Variadic because annotation values contain commas: FAYE_ATTR(Ecs::Range{0.f, 100.f}).
#if defined(FAYE_HAS_REFLECTION) && FAYE_HAS_REFLECTION
#  define FAYE_ATTR(...) [[=__VA_ARGS__]]
#else
#  define FAYE_ATTR(...)
#endif
