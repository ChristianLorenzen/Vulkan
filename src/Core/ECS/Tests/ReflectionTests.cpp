#include <doctest/doctest.h>

#include <string>
#include <string_view>
#include <vector>

#include <glm/glm.hpp>
#include <yaml-cpp/yaml.h>

#include "Core/ECS/Reflection/SemanticRegistry.hpp"
#include "Core/Handles/MaterialHandle.hpp"
#include "Core/Handles/ModelHandle.hpp"
#include "Core/Serialization/Uuid.hpp"
#include "engine/Scene/Serialization/Deserializer.hpp"
#include "engine/Scene/Serialization/Serializer.hpp"
#if defined(FAYE_HAS_REFLECTION) && FAYE_HAS_REFLECTION
#include "Core/ECS/Reflection/Describe.hpp"
#include "engine/Scene/Entities/Components.hpp"
#include "engine/Scene/Serialization/ComponentSerializers.hpp"
#include "Core/ECS/World.hpp"
#include "Renderer/PostProcess/PostProcessEffectLibrary.hpp"
#include "engine/Scene/Entities/RegisterComponents.hpp"
#include "engine/Scene/Serialization/ReflectedSerializers.hpp"
#endif

using namespace Faye;

// ---------------------------------------------------------------------------
// 1.4 -- SemanticRegistry
// ---------------------------------------------------------------------------
namespace
{
    // Push `src` through the registered ops for T and read it back into a
    // separate value, exactly as a generic walker would.
    template <class T>
    T semanticRoundTrip(const T &src, const T &startingValue = T{})
    {
        const Ecs::SemanticOps *ops =
            Ecs::SemanticRegistry::instance().find(Ecs::semanticIdOf<T>);
        REQUIRE(ops != nullptr);
        REQUIRE(ops->serialize != nullptr);
        REQUIRE(ops->deserialize != nullptr);

        YAML::Emitter emitter;
        Ecs::Serializer serializer(emitter);   // null registries (headless)
        emitter << YAML::BeginMap;
        ops->serialize(&src, "field", serializer);
        emitter << YAML::EndMap;

        const YAML::Node node = YAML::Load(emitter.c_str());
        REQUIRE(node.IsMap());

        T dst = startingValue;
        Ecs::Deserializer deserializer(node);
        ops->deserialize(&dst, "field", deserializer);
        return dst;
    }
}

TEST_CASE("registry exposes ops for every built-in semantic")
{
    const Ecs::SemanticRegistry &registry = Ecs::SemanticRegistry::instance();
    CHECK(registry.size() == 11);

    CHECK(registry.find(Ecs::hashTypeName("faye.f32")) != nullptr);
    CHECK(registry.find(Ecs::hashTypeName("glm.vec3")) != nullptr);
    CHECK(registry.find(Ecs::hashTypeName("faye.ModelAsset")) != nullptr);
    CHECK(registry.find(Ecs::hashTypeName("faye.MaterialAsset")) != nullptr);

    // Unregistered ids must report a miss, not crash: that is the signal for
    // "fall back to Layout".
    CHECK(registry.find(Ecs::hashTypeName("nothing.registered")) == nullptr);
    CHECK(registry.find(0) == nullptr);
}

TEST_CASE("model and material semantics are distinct ids")
{
    // The reason a single AssetRef layout tag was not enough: writeAssetField
    // has two overloads resolving through different registries, and the TypeId
    // is what preserves which.
    CHECK(Ecs::semanticIdOf<ModelHandle> != Ecs::semanticIdOf<MaterialHandle>);
    CHECK(Ecs::SemanticRegistry::instance().find(Ecs::semanticIdOf<ModelHandle>) !=
          Ecs::SemanticRegistry::instance().find(Ecs::semanticIdOf<MaterialHandle>));
}

TEST_CASE("scalar semantics round-trip through YAML")
{
    CHECK(semanticRoundTrip<bool>(true) == true);
    CHECK(semanticRoundTrip<bool>(false, true) == false);
    CHECK(semanticRoundTrip<int32_t>(-42) == -42);
    CHECK(semanticRoundTrip<uint32_t>(4000000000u) == 4000000000u);
    CHECK(semanticRoundTrip<float>(2.5f) == 2.5f);
    CHECK(semanticRoundTrip<std::string>("hello world") == "hello world");
}

TEST_CASE("vector semantics round-trip through YAML")
{
    CHECK(semanticRoundTrip(glm::vec2{1.0f, 2.0f}) == glm::vec2{1.0f, 2.0f});
    CHECK(semanticRoundTrip(glm::vec3{1.0f, -2.0f, 3.5f}) == glm::vec3{1.0f, -2.0f, 3.5f});
    CHECK(semanticRoundTrip(glm::vec4{1.0f, 2.0f, 3.0f, 4.0f}) == glm::vec4{1.0f, 2.0f, 3.0f, 4.0f});
}

TEST_CASE("uuid semantics round-trip through YAML")
{
    const Uuid id = Uuid::nameBased("faye:test-asset");
    CHECK(semanticRoundTrip(id) == id);
}

TEST_CASE("asset handles serialize as ids and resolve to null without a registry")
{
    // Headless: the null registry means the handle cannot be resolved on the
    // way back, which is exactly the documented behaviour.
    const ModelHandle model{7};
    CHECK(semanticRoundTrip(model).isValid() == false);

    YAML::Emitter emitter;
    Ecs::Serializer serializer(emitter);
    emitter << YAML::BeginMap;
    Ecs::SemanticRegistry::instance().find(Ecs::semanticIdOf<ModelHandle>)
        ->serialize(&model, "modelHandle", serializer);
    emitter << YAML::EndMap;

    const YAML::Node node = YAML::Load(emitter.c_str());
    CHECK(node["modelHandle"].IsDefined());
}

TEST_CASE("a missing key leaves the current value untouched")
{
    const Ecs::SemanticOps *ops =
        Ecs::SemanticRegistry::instance().find(Ecs::semanticIdOf<float>);
    REQUIRE(ops != nullptr);

    const YAML::Node node = YAML::Load("{other: 1.0}");
    Ecs::Deserializer deserializer(node);

    float value = 12.5f;
    ops->deserialize(&value, "absent", deserializer);
    CHECK(value == 12.5f);   // fallback is the current value, not T{}
}

TEST_CASE("defaultInto and equals are wired for every built-in")
{
    const Ecs::SemanticOps *ops =
        Ecs::SemanticRegistry::instance().find(Ecs::semanticIdOf<glm::vec3>);
    REQUIRE(ops != nullptr);

    glm::vec3 value{1.0f, 2.0f, 3.0f};
    const glm::vec3 other{1.0f, 2.0f, 3.0f};
    CHECK(ops->equals(&value, &other));

    ops->defaultInto(&value);
    CHECK(value == glm::vec3{});
    CHECK_FALSE(ops->equals(&value, &other));
}

TEST_CASE("draw stays null until the inspector lands in 1.11")
{
    CHECK(Ecs::SemanticRegistry::instance()
              .find(Ecs::semanticIdOf<float>)->draw == nullptr);
}

// ---------------------------------------------------------------------------
// 1.3 -- Describe.hpp. Reflection-only; the whole file must still build (and
// the 1.4 cases above must still run) with -DFAYE_ENABLE_REFLECTION=OFF.
// ---------------------------------------------------------------------------
#if defined(FAYE_HAS_REFLECTION) && FAYE_HAS_REFLECTION

namespace
{
    // The header deliberately ships no per-T binder (it would drag <meta> into
    // every includer). Tests bind locally instead -- the same two lines a
    // registration site would write.
    template <class T> void serializeAs(const void *c, Ecs::Serializer &s)
    { Ecs::serializeReflected(Ecs::kDescriptor<T>, c, s); }
    template <class T> void deserializeAs(void *c, Ecs::Deserializer &d)
    { Ecs::deserializeReflected(Ecs::kDescriptor<T>, c, d); }
}

namespace
{
    struct Inner
    {
        float a = 0.0f;
        float b = 0.0f;
    };

    // Supplies the `choice` dropdown below. Named by reflection from the
    // annotation, so renaming it breaks the build rather than degrading the
    // field to a text box at runtime.
    void probeChoiceOptions(const Ecs::OptionSink &sink)
    {
        sink.add("alpha", "Alpha");
        sink.add("beta");   // no label: the value is shown
    }

    enum class ProbeMode : uint8_t { Off, Linear, Smooth = 7 };

    struct Titled
    {
        std::string name{};
        float weight = 0.0f;
    };

    struct Probe
    {
        Inner nested{};
        FAYE_ATTR(Faye::Ecs::Range{0.0f, 100.0f})
        FAYE_ATTR(Faye::Ecs::Units("m"))
        float ranged = 1.0f;
        FAYE_ATTR(Faye::Ecs::NotSerialized) int cache = 0;
        FAYE_ATTR(Faye::Ecs::SerializedAs("oldTint"))
        FAYE_ATTR(Faye::Ecs::ColorPicker)
        glm::vec3 tint{};
        FAYE_ATTR(Faye::Ecs::HideInInspector) FAYE_ATTR(Faye::Ecs::ReadOnly) uint32_t internal = 0;
        FAYE_ATTR(Faye::Ecs::Radians)
        FAYE_ATTR(Faye::Ecs::Category("Advanced"))
        float angle = 0.0f;
        FAYE_ATTR(Faye::Ecs::Tooltip("some explanatory text")) bool enabled = false;
        FAYE_ATTR(Faye::Ecs::Orderable) std::vector<Inner> structs{};
        std::vector<float> scalars{};
        std::string label{};
        ModelHandle model{};
        std::vector<std::string> names{};
        std::vector<ModelHandle> models{};
        std::vector<std::vector<float>> grid{};
        FAYE_ATTR(Faye::Ecs::OptionsFrom{^^probeChoiceOptions}) std::string choice{};
        ProbeMode mode = ProbeMode::Off;                          // no annotation: enum drives it
        FAYE_ATTR(Faye::Ecs::TitleFrom{^^Titled::name}) std::vector<Titled> titled{};
        bool gate = true;
        FAYE_ATTR(Faye::Ecs::VisibleIf{^^Probe::gate}) float shownWhenGate = 1.0f;
        FAYE_ATTR(Faye::Ecs::HiddenIf{^^Probe::gate}) float hiddenWhenGate = 2.0f;
    };

    // Field indices into Probe's descriptor, so the assertions below read as
    // something other than a wall of magic numbers. Append only -- inserting
    // renumbers every assertion below.
    enum ProbeField : uint32_t {
        Nested, Ranged, Cache, Tint, Internal, Angle, Enabled,
        Structs, Scalars, Label, Model, Names, Models, Grid, Choice,
        Mode, TitledList, Gate, ShownWhenGate, HiddenWhenGate, ProbeFieldCount
    };

    struct FAYE_ATTR(Faye::Ecs::TypeName("faye.Renamed")) Renamed
    {
        int value = 0;
    };
}

TEST_CASE("describe reports the real field list")
{
    constexpr const Ecs::TypeDescriptor &d = Ecs::describe<TransformComponent>();

    static_assert(d.fieldCount == 4);
    static_assert(std::string_view{d.fields[0].name} == "translation");
    static_assert(d.size == sizeof(TransformComponent));
    static_assert(d.alignment == alignof(TransformComponent));

    // TypeName wins over identifier_of: the C++ type is TransformComponent but
    // the persisted key is "Transform", which is what .faye files contain. The
    // identity guard at the bottom of this file asserts the pair stays in sync
    // with the registry.
    static_assert(std::string_view{d.name} == "Transform");
    static_assert(d.id == Ecs::hashTypeName("Transform"));

    // Offsets must match the real layout, not the declaration order alone.
    static_assert(d.fields[1].offset == offsetof(TransformComponent, scale));
    static_assert(d.fields[2].offset == offsetof(TransformComponent, rotation));

    // constructDefault must run the default member initialisers, not just
    // zero the storage -- scale defaults to 1, not 0.
    alignas(TransformComponent) unsigned char storage[sizeof(TransformComponent)]{};
    d.constructDefault(storage);
    const TransformComponent &built = *reinterpret_cast<TransformComponent *>(storage);
    CHECK(built.scale == glm::vec3{1.0f, 1.0f, 1.0f});
    CHECK(built.translation == glm::vec3{});
}

TEST_CASE("layout and semantic id are independent axes")
{
    constexpr const Ecs::TypeDescriptor &d = Ecs::describe<Probe>();

    // vec3 -> both a layout and a semantic.
    static_assert(d.fields[Tint].layout == Ecs::Layout::F32x3);
    static_assert(d.fields[Tint].typeId == Ecs::hashTypeName("glm.vec3"));

    // ModelHandle -> semantic only. It is an aggregate wrapping a uint32_t, so
    // without the semantic-is-a-leaf rule the walker would descend into it and
    // emit `value: 7` instead of an asset id.
    static_assert(d.fields[Model].layout == Ecs::Layout::Opaque);
    static_assert(d.fields[Model].typeId == Ecs::hashTypeName("faye.ModelAsset"));
    static_assert(d.fields[Model].nested == nullptr);
    CHECK(true);
}

TEST_CASE("nested aggregates recurse, leaves do not")
{
    constexpr const Ecs::TypeDescriptor &d = Ecs::describe<Probe>();

    static_assert(d.fields[Nested].layout == Ecs::Layout::Struct);
    static_assert(d.fields[Nested].nested->fieldCount == 2);
    static_assert(std::string_view{d.fields[Nested].nested->fields[1].name} == "b");
    static_assert(d.fields[Nested].nested == &Ecs::kDescriptor<Inner>);
    static_assert(d.fields[Nested].element == nullptr);   // not a container
    CHECK(true);
}

TEST_CASE("a vector element can be anything a field can be")
{
    constexpr const Ecs::TypeDescriptor &d = Ecs::describe<Probe>();
    static_assert(d.fieldCount == ProbeFieldCount);

    // Every container looks the same at the container level...
    for (const uint32_t i : {Structs, Scalars, Names, Models, Grid})
    {
        INFO("field: ", std::string{d.fields[i].name});
        CHECK(d.fields[i].layout == Ecs::Layout::DynArray);
        CHECK(d.fields[i].elemCount == 0);
        CHECK(d.fields[i].ops != nullptr);
        CHECK(d.fields[i].element != nullptr);
        CHECK(d.fields[i].nested == nullptr);   // `nested` is Struct-only now
    }

    // ...and the element descriptor carries whatever that element needs.
    static_assert(d.fields[Structs].element->layout == Ecs::Layout::Struct);
    static_assert(d.fields[Structs].element->nested == &Ecs::kDescriptor<Inner>);
    static_assert(d.fields[Structs].element->size == sizeof(Inner));

    static_assert(d.fields[Scalars].element->layout == Ecs::Layout::F32);
    static_assert(d.fields[Scalars].element->typeId == Ecs::hashTypeName("faye.f32"));

    static_assert(d.fields[Names].element->layout == Ecs::Layout::String);
    static_assert(d.fields[Names].element->typeId == Ecs::hashTypeName("faye.string"));
    // Elements need ops too, and they are the SAME ops a plain string field gets.
    static_assert(d.fields[Names].element->ops == d.fields[Label].ops);

    static_assert(d.fields[Models].element->layout == Ecs::Layout::Opaque);
    static_assert(d.fields[Models].element->typeId == Ecs::hashTypeName("faye.ModelAsset"));

    // Containers of containers: the element is itself a DynArray with an element.
    static_assert(d.fields[Grid].element->layout == Ecs::Layout::DynArray);
    static_assert(d.fields[Grid].element->elemCount == 0);
    static_assert(d.fields[Grid].element->element->layout == Ecs::Layout::F32);
    // The inner vector<float> is described identically whether it appears as a
    // field or as an element -- one definition, no drift.
    static_assert(d.fields[Grid].element->ops == d.fields[Scalars].ops);
    static_assert(d.fields[Grid].element->element == d.fields[Scalars].element);

    // Elements are addressed through ops, never by offset, so these stay zero.
    static_assert(d.fields[Scalars].element->name == nullptr);
    static_assert(d.fields[Scalars].element->offset == 0);
    static_assert(d.fields[Scalars].element->flags == Ecs::FieldFlags::None);
}

TEST_CASE("container ops walk every element type against a live object")
{
    constexpr const Ecs::TypeDescriptor &d = Ecs::describe<Probe>();
    Probe probe;
    const auto at = [&probe](const Ecs::FieldDescriptor &f) -> void *
    { return reinterpret_cast<char *>(&probe) + f.offset; };

    const Ecs::FieldDescriptor &structs = d.fields[Structs];
    structs.ops->resize(at(structs), 3);
    CHECK(structs.ops->size(at(structs)) == 3);
    static_cast<Inner *>(structs.ops->elementAt(at(structs), 1))->a = 9.0f;
    CHECK(probe.structs[1].a == 9.0f);
    structs.ops->clear(at(structs));
    CHECK(probe.structs.empty());

    const Ecs::FieldDescriptor &names = d.fields[Names];
    names.ops->resize(at(names), 2);
    *static_cast<std::string *>(names.ops->elementAt(at(names), 0)) = "first";
    CHECK(probe.names[0] == "first");
    // The element's OWN ops walk one level further down.
    CHECK(names.element->ops->size(names.ops->elementAt(at(names), 0)) == 5);

    const Ecs::FieldDescriptor &models = d.fields[Models];
    models.ops->resize(at(models), 1);
    *static_cast<ModelHandle *>(models.ops->elementAt(at(models), 0)) = ModelHandle{7};
    CHECK(probe.models[0].value == 7);

    // Nested containers: outer ops to reach the inner vector, inner ops to fill it.
    const Ecs::FieldDescriptor &grid = d.fields[Grid];
    grid.ops->resize(at(grid), 2);
    void *row = grid.ops->elementAt(at(grid), 1);
    grid.element->ops->resize(row, 4);
    *static_cast<float *>(grid.element->ops->elementAt(row, 3)) = 2.5f;
    CHECK(probe.grid[1].size() == 4);
    CHECK(probe.grid[1][3] == 2.5f);

    const Ecs::FieldDescriptor &str = d.fields[Label];
    probe.label = "abc";
    CHECK(str.ops->size(at(str)) == 3);
    CHECK(*static_cast<char *>(str.ops->elementAt(at(str), 2)) == 'c');
    str.ops->clear(at(str));
    CHECK(probe.label.empty());
}

TEST_CASE("an OptionsFrom provider is callable straight off the descriptor")
{
    // The point of splicing the reflection instead of storing a name: the
    // editor calls this with no registry, no lookup, and no way for the
    // annotation and the provider to drift apart.
    constexpr const Ecs::TypeDescriptor &d = Ecs::describe<Probe>();

    std::vector<Ecs::FieldOption> collected;
    const Ecs::OptionSink sink{
        &collected,
        [](void *context, Ecs::FieldOption option) {
            static_cast<std::vector<Ecs::FieldOption> *>(context)->push_back(option);
        }};

    REQUIRE(d.fields[Choice].ui.options != nullptr);
    d.fields[Choice].ui.options(sink);

    REQUIRE(collected.size() == 2);
    CHECK(std::string_view{collected[0].value} == "alpha");
    CHECK(std::string_view{collected[0].label} == "Alpha");
    CHECK(std::string_view{collected[1].value} == "beta");
    CHECK(collected[1].label == nullptr);   // caller falls back to the value
}

TEST_CASE("an enum carries its name table, with no annotation")
{
    // "The type chooses the widget, the annotation refines it." An enum's legal
    // values ARE its enumerators, so requiring an opt-in here would be noise.
    //
    // The table lives on the FieldDescriptor rather than in FieldUI because the
    // serializer needs it too -- one table feeding both is what stops the
    // dropdown and the save file from ever disagreeing about an enumerator.
    constexpr const Ecs::TypeDescriptor &d = Ecs::describe<Probe>();

    // Layout stays the underlying integer: how it is stored has not changed.
    static_assert(d.fields[Mode].layout == Ecs::Layout::U8);
    REQUIRE(d.fields[Mode].enumOps != nullptr);

    const Ecs::EnumOps &ops = *d.fields[Mode].enumOps;
    REQUIRE(ops.count == 3);
    CHECK(std::string_view{ops.entries[0].name} == "Off");
    CHECK(ops.entries[0].value == 0);
    CHECK(std::string_view{ops.entries[2].name} == "Smooth");
    CHECK(ops.entries[2].value == 7);   // the declared value, not the index

    CHECK(std::string_view{ops.nameOf(7)} == "Smooth");
    CHECK(ops.nameOf(99) == nullptr);   // a value from a newer build

    int64_t resolved = -1;
    CHECK(ops.valueOf("Linear", resolved));
    CHECK(resolved == 1);
    CHECK_FALSE(ops.valueOf("Nonexistent", resolved));
    CHECK_FALSE(ops.valueOf("Line", resolved));   // prefix must not match

    // read/write go through the real width rather than reinterpreting a pointer.
    Probe probe{};
    void *slot = reinterpret_cast<char *>(&probe) + d.fields[Mode].offset;
    ops.write(slot, 7);
    CHECK(probe.mode == ProbeMode::Smooth);
    CHECK(ops.read(slot) == 7);

    // Non-enums carry no table.
    CHECK(d.fields[Ranged].enumOps == nullptr);
    CHECK(d.fields[Choice].enumOps == nullptr);
}

TEST_CASE("enums persist by name, not by ordinal")
{
    // By name so a save survives someone inserting an enumerator in the middle
    // -- an ordinal would silently reinterpret every existing scene. This is
    // the rule MaterialEnumNames.cpp already follows by hand.
    TransformComponent written{};
    written.enumVal = TESTTWO;

    YAML::Emitter emitter;
    Ecs::Serializer s(emitter);   // null registries (headless)
    emitter << YAML::BeginMap;
    Ecs::serializeReflected(Ecs::describe<TransformComponent>(), &written, s);
    emitter << YAML::EndMap;
    const std::string text = emitter.c_str();

    INFO(text);
    CHECK(text.find("enumVal: TESTTWO") != std::string::npos);
    CHECK(text.find("enumVal: 1") == std::string::npos);

    TransformComponent read{};
    Ecs::Deserializer d{YAML::Load(text)};
    Ecs::deserializeReflected(Ecs::describe<TransformComponent>(), &read, d);
    CHECK(read.enumVal == TESTTWO);

    SUBCASE("an unknown name keeps the constructed default rather than zeroing")
    {
        TransformComponent fallback{};
        fallback.enumVal = FINAL;
        Ecs::Deserializer stale{YAML::Load("{enumVal: REMOVED_IN_A_LATER_BUILD}")};
        Ecs::deserializeReflected(Ecs::describe<TransformComponent>(), &fallback, stale);
        CHECK(fallback.enumVal == FINAL);
    }

    SUBCASE("a numeric value still reads, so an int-cast save is not lost")
    {
        TransformComponent numeric{};
        Ecs::Deserializer node{YAML::Load("{enumVal: 2}")};
        Ecs::deserializeReflected(Ecs::describe<TransformComponent>(), &numeric, node);
        CHECK(numeric.enumVal == FINAL);
    }
}

TEST_CASE("TitleFrom labels container rows from the element")
{
    constexpr const Ecs::TypeDescriptor &d = Ecs::describe<Probe>();

    Probe probe{};
    probe.titled = {Titled{"first", 1.0f}, Titled{"second", 2.0f}};

    REQUIRE(d.fields[TitledList].ui.title != nullptr);
    const Ecs::FieldOps &ops = *d.fields[TitledList].ops;
    void *container = reinterpret_cast<char *>(&probe) + d.fields[TitledList].offset;

    CHECK(std::string_view{d.fields[TitledList].ui.title(ops.elementAt(container, 0))} == "first");
    CHECK(std::string_view{d.fields[TitledList].ui.title(ops.elementAt(container, 1))} == "second");

    // Follows the element, so reordering relabels the rows rather than the
    // titles going stale against their indices.
    ops.swapElements(container, 0, 1);
    CHECK(std::string_view{d.fields[TitledList].ui.title(ops.elementAt(container, 0))} == "second");

    // Containers without the annotation fall back to the index.
    CHECK(d.fields[Structs].ui.title == nullptr);
}

TEST_CASE("VisibleIf and HiddenIf read a sibling of the owning struct")
{
    constexpr const Ecs::TypeDescriptor &d = Ecs::describe<Probe>();

    Probe probe{};
    const Ecs::VisibilityFn shown = d.fields[ShownWhenGate].ui.visibleWhen;
    const Ecs::VisibilityFn hidden = d.fields[HiddenWhenGate].ui.visibleWhen;
    REQUIRE(shown != nullptr);
    REQUIRE(hidden != nullptr);

    // The predicate takes the OWNING struct, not the field -- which is what
    // confines a condition to siblings.
    probe.gate = true;
    CHECK(shown(&probe));
    CHECK_FALSE(hidden(&probe));

    probe.gate = false;
    CHECK_FALSE(shown(&probe));
    CHECK(hidden(&probe));

    // Unconditional fields carry no predicate rather than a always-true one, so
    // the walker can skip the call entirely.
    CHECK(d.fields[Gate].ui.visibleWhen == nullptr);
    CHECK(d.fields[Ranged].ui.visibleWhen == nullptr);
}

TEST_CASE("eraseAt and swapElements make a container editable, not just walkable")
{
    // resize/clear alone can only append and drop from the end. Without these
    // two, "remove this element" and "move it up" cannot be expressed
    // generically and every ordered list needs a hand-written drawer.
    constexpr const Ecs::TypeDescriptor &d = Ecs::describe<Probe>();

    Probe probe{};
    const auto at = [&probe](const Ecs::FieldDescriptor &f) {
        return static_cast<void *>(reinterpret_cast<char *>(&probe) + f.offset);
    };

    const Ecs::FieldDescriptor &scalars = d.fields[Scalars];
    REQUIRE(scalars.ops->eraseAt != nullptr);
    REQUIRE(scalars.ops->swapElements != nullptr);

    probe.scalars = {1.0f, 2.0f, 3.0f};
    scalars.ops->swapElements(at(scalars), 0, 2);
    CHECK(probe.scalars == std::vector<float>{3.0f, 2.0f, 1.0f});

    scalars.ops->eraseAt(at(scalars), 1);
    CHECK(probe.scalars == std::vector<float>{3.0f, 1.0f});

    // Both are bounds-checked rather than UB: the editor picks an index while
    // drawing and applies it after the loop, by which point the container may
    // already have changed.
    scalars.ops->eraseAt(at(scalars), 99);
    scalars.ops->swapElements(at(scalars), 0, 99);
    scalars.ops->swapElements(at(scalars), 1, 1);
    CHECK(probe.scalars == std::vector<float>{3.0f, 1.0f});

    // Non-trivially-copyable elements move by value, not by byte blit.
    const Ecs::FieldDescriptor &names = d.fields[Names];
    probe.names = {"first", "second"};
    names.ops->swapElements(at(names), 0, 1);
    CHECK(probe.names == std::vector<std::string>{"second", "first"});
    names.ops->eraseAt(at(names), 0);
    CHECK(probe.names == std::vector<std::string>{"first"});
}

TEST_CASE("every element type resolves to something a walker can act on")
{
    // The invariant 1.6 depends on: descending a container must always end at
    // a semantic (registry handles it), a Struct (recurse), or another
    // container (recurse). Never a dead end.
    constexpr const Ecs::TypeDescriptor &d = Ecs::describe<Probe>();

    const auto actionable = [](auto &&self, const Ecs::FieldDescriptor &f) -> bool
    {
        if (f.typeId != 0)
            return Ecs::SemanticRegistry::instance().find(f.typeId) != nullptr;
        if (f.layout == Ecs::Layout::Struct)
            return f.nested != nullptr;
        if (f.layout == Ecs::Layout::DynArray)
            return f.ops != nullptr && f.element != nullptr && self(self, *f.element);
        return false;
    };

    for (const uint32_t i : {Structs, Scalars, Names, Models, Grid})
    {
        INFO("field: ", std::string{d.fields[i].name});
        CHECK(actionable(actionable, d.fields[i]));
    }
}

TEST_CASE("annotations land in flags and ui, never in layout")
{
    constexpr const Ecs::TypeDescriptor &d = Ecs::describe<Probe>();

    static_assert(d.fields[Ranged].ui.hasRange);
    static_assert(d.fields[Ranged].ui.rangeMin == 0.0f);
    static_assert(d.fields[Ranged].ui.rangeMax == 100.0f);
    static_assert(std::string_view{d.fields[Ranged].ui.units} == "m");
    static_assert(d.fields[Ranged].flags == Ecs::FieldFlags::None);

    static_assert(hasFlag(d.fields[Cache].flags, Ecs::FieldFlags::NotSerialized));
    static_assert(std::string_view{d.fields[Tint].serializedAs} == "oldTint");
    static_assert(d.fields[Tint].ui.colorPicker);

    static_assert(hasFlag(d.fields[Internal].flags, Ecs::FieldFlags::HideInInspector));
    static_assert(hasFlag(d.fields[Internal].flags, Ecs::FieldFlags::ReadOnly));
    static_assert(!hasFlag(d.fields[Internal].flags, Ecs::FieldFlags::NotSerialized));

    static_assert(d.fields[Angle].ui.radians);
    static_assert(std::string_view{d.fields[Enabled].ui.tooltip} == "some explanatory text");

    // Category is inspector grouping only: it must not disturb the field's
    // flags, its layout, or the other annotations on the same member.
    static_assert(std::string_view{d.fields[Angle].ui.category} == "Advanced");
    static_assert(d.fields[Angle].layout == Ecs::Layout::F32);
    static_assert(d.fields[Angle].flags == Ecs::FieldFlags::None);
    static_assert(d.fields[Enabled].ui.category == nullptr);

    // OptionsFrom is spliced into the provider itself at describe time -- the
    // descriptor holds a callable function pointer, not a name to look up.
    static_assert(d.fields[Choice].ui.options == &probeChoiceOptions);
    static_assert(d.fields[Choice].layout == Ecs::Layout::String);
    static_assert(d.fields[Ranged].ui.options == nullptr);

    // Orderable is per FIELD. `structs` opts in; `scalars` and `grid` sit in
    // the same struct with the same kind of container and do not.
    static_assert(d.fields[Structs].ui.orderable);
    static_assert(!d.fields[Scalars].ui.orderable);
    static_assert(!d.fields[Grid].ui.orderable);
    // ...and it must not leak into the ELEMENT descriptor: an orderable vector
    // of vectors does not make its rows orderable.
    static_assert(!d.fields[Structs].element->ui.orderable);

    // Unannotated fields carry nothing.
    static_assert(d.fields[Nested].serializedAs == nullptr);
    static_assert(d.fields[Nested].ui.tooltip == nullptr);
    static_assert(d.fields[Nested].ui.units == nullptr);
    static_assert(d.fields[Nested].ui.category == nullptr);
    static_assert(!d.fields[Nested].ui.hasRange);
    CHECK(true);
}

TEST_CASE("TypeName overrides the C++ identifier")
{
    constexpr const Ecs::TypeDescriptor &d = Ecs::describe<Renamed>();
    static_assert(std::string_view{d.name} == "faye.Renamed");
    static_assert(d.id == Ecs::hashTypeName("faye.Renamed"));
    CHECK(true);
}

TEST_CASE("every described field with a semantic has registered ops")
{
    // The invariant that makes 1.6's dispatch safe: if describe() stamped a
    // non-zero TypeId, the registry must know how to handle it.
    constexpr const Ecs::TypeDescriptor &d = Ecs::describe<Probe>();
    for (uint32_t i = 0; i < d.fieldCount; ++i)
    {
        const Ecs::FieldDescriptor &f = d.fields[i];
        if (f.typeId != 0)
        {
            INFO("field: ", std::string{f.name});
            CHECK(Ecs::SemanticRegistry::instance().find(f.typeId) != nullptr);
        }
    }
}

// ---------------------------------------------------------------------------
// 1.6 -- generic reflected serializers
// ---------------------------------------------------------------------------
namespace
{
    struct Leaf
    {
        float a = 0.0f;
        std::string tag{};
    };

    struct Wide
    {
        glm::vec3 position{1.0f, 2.0f, 3.0f};
        float scale = 2.5f;
        FAYE_ATTR(Faye::Ecs::NotSerialized) int cache = 99;
        FAYE_ATTR(Faye::Ecs::SerializedAs("oldName")) std::string name{"default"};
        Leaf leaf{};
        std::vector<float> scalars{};
        std::vector<std::string> words{};
        std::vector<Leaf> leaves{};
        std::vector<std::vector<float>> grid{};
        FAYE_ATTR(Faye::Ecs::Range{0.0f, 1.0f}) float ranged = 5.0f;   // out of range on purpose
    };

    std::string emitReflected(const Wide &src)
    {
        YAML::Emitter emitter;
        Ecs::Serializer serializer(emitter);
        emitter << YAML::BeginMap;
        serializeAs<Wide>(&src, serializer);
        emitter << YAML::EndMap;
        return emitter.c_str();
    }

    Wide roundTripReflected(const Wide &src, const Wide &startingValue = Wide{})
    {
        const YAML::Node node = YAML::Load(emitReflected(src));
        REQUIRE(node.IsMap());
        Wide dst = startingValue;
        Ecs::Deserializer deserializer(node);
        deserializeAs<Wide>(&dst, deserializer);
        return dst;
    }
}

TEST_CASE("reflected round-trip preserves scalars and structs")
{
    Wide src;
    src.position = {4.0f, -5.0f, 6.5f};
    src.scale = 0.25f;
    src.name = "renamed";
    src.leaf = {7.5f, "leafy"};

    const Wide dst = roundTripReflected(src);
    CHECK(dst.position == src.position);
    CHECK(dst.scale == src.scale);
    CHECK(dst.name == src.name);
    CHECK(dst.leaf.a == src.leaf.a);
    CHECK(dst.leaf.tag == src.leaf.tag);
}

TEST_CASE("NotSerialized fields never reach the file")
{
    Wide src;
    src.cache = 12345;

    CHECK(emitReflected(src).find("cache") == std::string::npos);

    Wide dst;
    dst.cache = 7;
    dst = roundTripReflected(src, dst);
    CHECK(dst.cache == 7);   // untouched, not reset and not copied
}

TEST_CASE("vectors of every element type round-trip")
{
    Wide src;
    src.scalars = {1.0f, 2.5f, -3.0f};
    src.words = {"alpha", "beta"};
    src.leaves = {{1.0f, "one"}, {2.0f, "two"}};
    src.grid = {{1.0f, 2.0f}, {}, {3.0f}};

    const Wide dst = roundTripReflected(src);

    CHECK(dst.scalars == src.scalars);
    CHECK(dst.words == src.words);

    REQUIRE(dst.leaves.size() == 2);
    CHECK(dst.leaves[1].a == 2.0f);
    CHECK(dst.leaves[1].tag == "two");

    REQUIRE(dst.grid.size() == 3);
    CHECK(dst.grid[0] == std::vector<float>{1.0f, 2.0f});
    CHECK(dst.grid[1].empty());
    CHECK(dst.grid[2] == std::vector<float>{3.0f});
}

TEST_CASE("a populated vector is replaced, not appended to")
{
    Wide src;
    src.scalars = {9.0f};

    Wide dst;
    dst.scalars = {1.0f, 2.0f, 3.0f, 4.0f};
    dst = roundTripReflected(src, dst);

    CHECK(dst.scalars == std::vector<float>{9.0f});
}

TEST_CASE("SerializedAs reads an old key, and only as a fallback")
{
    Wide dst;
    SUBCASE("alias alone is honoured")
    {
        const YAML::Node node = YAML::Load("{oldName: fromAlias}");
        Ecs::Deserializer d(node);
        deserializeAs<Wide>(&dst, d);
        CHECK(dst.name == "fromAlias");
    }
    SUBCASE("primary key wins when both are present")
    {
        const YAML::Node node = YAML::Load("{name: primary, oldName: fromAlias}");
        Ecs::Deserializer d(node);
        deserializeAs<Wide>(&dst, d);
        CHECK(dst.name == "primary");
    }
}

TEST_CASE("a missing key leaves the constructed default in place")
{
    const YAML::Node node = YAML::Load("{scale: 8.0}");
    Ecs::Deserializer d(node);

    Wide dst;
    dst.position = {5.0f, 5.0f, 5.0f};
    dst.leaf.tag = "kept";
    dst.scalars = {1.0f};
    deserializeAs<Wide>(&dst, d);

    CHECK(dst.scale == 8.0f);
    CHECK(dst.position == glm::vec3{5.0f, 5.0f, 5.0f});
    CHECK(dst.leaf.tag == "kept");
    CHECK(dst.scalars == std::vector<float>{1.0f});   // absent sequence != empty sequence
}

TEST_CASE("annotations never affect bytes")
{
    // ranged is annotated [0,1] and set to 5. Clamping on read would mean a
    // widened Range later changes how old scenes load.
    Wide src;
    src.ranged = 5.0f;
    CHECK(roundTripReflected(src).ranged == 5.0f);
}

TEST_CASE("reflected and hand-written serializers are wire compatible")
{
    // The 1.7 compatibility guarantee, checked early against the component
    // whose hand-written thunk is already in its final form.
    //
    // NOTE: this is deliberately NOT a byte-identical check. The reflected
    // walker emits fields in DECLARATION order (translation, scale, rotation);
    // serializeTransform emits translation, rotation, scale. Same keys, same
    // values, different order -- so scenes still load either way, but 1.7 will
    // reorder keys the first time each scene is re-saved. Either accept that
    // one-time diff or reorder the hand-written thunks to match declaration
    // order before migrating; do not "fix" it by reordering struct members.
    TransformComponent src;
    src.translation = {1.0f, 2.0f, 3.0f};
    src.rotation = {0.5f, 0.0f, -1.25f};
    src.scale = {2.0f, 2.0f, 2.0f};

    const auto emit = [&src](auto &&thunk) {
        YAML::Emitter emitter;
        Ecs::Serializer serializer(emitter);
        emitter << YAML::BeginMap;
        thunk(&src, serializer);
        emitter << YAML::EndMap;
        return YAML::Load(emitter.c_str());
    };

    const YAML::Node reflected = emit(serializeAs<TransformComponent>);
    const YAML::Node handWritten = emit(Ecs::serializeTransform);

    // Same key set, same values -- order is the only difference.
    REQUIRE(reflected.size() == handWritten.size());
    for (const auto &entry : handWritten)
    {
        const std::string key = entry.first.as<std::string>();
        INFO("key: ", key);
        REQUIRE(reflected[key].IsDefined());
        CHECK(Ecs::Deserializer::parseVec3(reflected[key], {}) ==
              Ecs::Deserializer::parseVec3(entry.second, {}));
    }

    // Each writer's output must load through the OTHER reader.
    const auto readBack = [](const YAML::Node &node, auto &&thunk) {
        TransformComponent dst;
        Ecs::Deserializer d(node);
        thunk(&dst, d);
        return dst;
    };

    const TransformComponent viaReflected =
        readBack(handWritten, deserializeAs<TransformComponent>);
    CHECK(viaReflected.translation == src.translation);
    CHECK(viaReflected.rotation == src.rotation);
    CHECK(viaReflected.scale == src.scale);

    const TransformComponent viaHandWritten = readBack(reflected, Ecs::deserializeTransform);
    CHECK(viaHandWritten.translation == src.translation);
    CHECK(viaHandWritten.rotation == src.rotation);
    CHECK(viaHandWritten.scale == src.scale);
}

TEST_CASE("isSerializableField gates whole fields, never partial sequences")
{
    constexpr const Ecs::TypeDescriptor &d = Ecs::describe<Wide>();
    for (uint32_t i = 0; i < d.fieldCount; ++i)
    {
        INFO("field: ", std::string{d.fields[i].name});
        CHECK(Ecs::isSerializableField(d.fields[i]));
    }
}

// ---------------------------------------------------------------------------
// 1.7 -- migration parity harness.
//
// Run this before and after converting each component. It compares the
// descriptor-driven walk against the hand-written thunk that is being replaced,
// and checks that each writer's output loads through the OTHER reader -- which
// is the guarantee that matters for existing .faye files.
//
// `bytes` records whether the two agree character-for-character. Key ORDER is
// the only thing that can differ: the reflected walk emits fields in
// declaration order, so a thunk that emits them in another order is
// semantically identical but textually different. Fix the thunk's order (never
// the struct's) to promote a component to Bytes::identical.
// ---------------------------------------------------------------------------
namespace
{
    enum class Bytes { identical, orderDiffers };

    template <class T, class SerFn, class DeserFn>
    void checkParity(const char *label, const T &src, SerFn handWrittenSer,
                     DeserFn handWrittenDeser, Bytes bytes)
    {
        const auto emit = [&src](auto &&thunk) {
            YAML::Emitter e;
            Ecs::Serializer s(e);
            e << YAML::BeginMap;
            thunk(&src, s);
            e << YAML::EndMap;
            return std::string{e.c_str()};
        };
        const std::string reflectedText = emit(serializeAs<T>);
        const std::string handWrittenText = emit(handWrittenSer);

        INFO(label, "\n--- reflected ---\n", reflectedText,
                    "\n--- hand-written ---\n", handWrittenText);

        const YAML::Node reflected = YAML::Load(reflectedText);
        const YAML::Node handWritten = YAML::Load(handWrittenText);

        // Same keys, whatever the order.
        std::vector<std::string> reflectedKeys, handWrittenKeys;
        for (const auto &kv : reflected) reflectedKeys.push_back(kv.first.as<std::string>());
        for (const auto &kv : handWritten) handWrittenKeys.push_back(kv.first.as<std::string>());
        std::sort(reflectedKeys.begin(), reflectedKeys.end());
        std::sort(handWrittenKeys.begin(), handWrittenKeys.end());
        CHECK(reflectedKeys == handWrittenKeys);

        if (bytes == Bytes::identical)
            CHECK(reflectedText == handWrittenText);

        // Cross-compatibility -- the guarantee that protects scenes already on
        // disk. Read each writer's output with the OTHER reader, re-emit through
        // the reflected writer, and require the same text: that can only hold if
        // both readers recovered identical values.
        const auto reemit = [&emit](const T &value) {
            return emit([&value](const void *, Ecs::Serializer &s) { serializeAs<T>(&value, s); });
        };
        const auto readWith = [](const YAML::Node &node, auto &&thunk) {
            T dst{};
            Ecs::Deserializer d(node);
            thunk(&dst, d);
            return dst;
        };

        CHECK(reemit(readWith(handWritten, deserializeAs<T>)) == reflectedText);
        CHECK(reemit(readWith(reflected, handWrittenDeser)) == reflectedText);
    }
}

TEST_CASE("engine components: reflected output matches the hand-written thunks")
{
    TransformComponent transform;
    transform.translation = {1.0f, 2.0f, 3.0f};
    transform.rotation = {0.5f, 0.0f, -1.25f};
    transform.scale = {2.0f, 2.0f, 2.0f};
    checkParity("Transform", transform, Ecs::serializeTransform,
                Ecs::deserializeTransform, Bytes::identical);

    // Camera: `camera` is an opaque runtime type opted out with NotSerialized,
    // so only `primary` crosses the wire -- matching serializeCamera exactly.
    CameraComponent camera;
    camera.primary = true;
    checkParity("Camera", camera, Ecs::serializeCamera,
                Ecs::deserializeCamera, Bytes::identical);

    MeshRendererComponent mesh;
    mesh.view = false;
    checkParity("Mesh", mesh, Ecs::serializeMesh, Ecs::deserializeMesh, Bytes::identical);

    WaterComponent water;
    water.subdivisions = 32;
    checkParity("Water", water, Ecs::serializeWater, Ecs::deserializeWater, Bytes::identical);

    PointLightComponent point;
    point.color = {1.0f, 0.5f, 0.25f};
    point.intensity = 4.5f;
    point.radius = 2.0f;
    checkParity("Point Light", point, Ecs::serializePointLight,
                Ecs::deserializePointLight, Bytes::identical);

    DirectionalLightComponent directional;
    directional.color = {0.2f, 0.3f, 0.4f};
    directional.intensity = 3.0f;
    checkParity("Directional Light", directional, Ecs::serializeDirectionalLight,
                Ecs::deserializeDirectionalLight, Bytes::identical);

    RigidBody2dComponent body;
    body.velocity = {1.0f, 2.0f};
    body.mass = 5.0f;
    checkParity("RigidBody2D", body, Ecs::serializeRigidBody2d,
                Ecs::deserializeRigidBody2d, Bytes::identical);

    // The nested case: a vector of structs, each with a string and a nested
    // parameter block. Byte-identical through the generic walk.
    PostProcessStackComponent stack;
    stack.enabled = true;
    PostProcessEffectComponent effect;
    effect.definitionId = "bloom";
    effect.enabled = true;
    effect.parameters.color = {1.0f, 2.0f, 3.0f, 4.0f};
    effect.parameters.params = {5.0f, 6.0f, 7.0f, 8.0f};
    stack.effects.push_back(effect);
    checkParity("Post Process Stack", stack, Ecs::serializePostProcessStack,
                Ecs::deserializePostProcessStack, Bytes::identical);
}

TEST_CASE("every engine component is reachable by the generic walk")
{
    // Fails the moment a new field lands that has no semantic and is not a
    // describable struct -- i.e. before it can silently vanish from a save.
    const auto allFieldsOk = [](const Ecs::TypeDescriptor &d) {
        for (uint32_t i = 0; i < d.fieldCount; ++i)
        {
            const Ecs::FieldDescriptor &f = d.fields[i];
            if (hasFlag(f.flags, Ecs::FieldFlags::NotSerialized))
                continue;   // opted out on purpose
            INFO("field: ", std::string{f.name});
            if (!Ecs::isSerializableField(f))
                return false;
        }
        return true;
    };

    CHECK(allFieldsOk(Ecs::describe<TransformComponent>()));
    CHECK(allFieldsOk(Ecs::describe<MeshRendererComponent>()));
    CHECK(allFieldsOk(Ecs::describe<WaterComponent>()));
    CHECK(allFieldsOk(Ecs::describe<PointLightComponent>()));
    CHECK(allFieldsOk(Ecs::describe<DirectionalLightComponent>()));
    CHECK(allFieldsOk(Ecs::describe<PostProcessStackComponent>()));
    CHECK(allFieldsOk(Ecs::describe<RigidBody2dComponent>()));
    CHECK(allFieldsOk(Ecs::describe<CameraComponent>()));
}

TEST_CASE("descriptor identity agrees with the registry")
{
    // The trap this guards: TypeName is the PERSISTED key. If a descriptor's
    // name drifts from the registered name, `type:` in every .faye file stops
    // matching the moment registration starts deriving the name from the
    // descriptor -- silently, and only for scenes saved before the drift.
    //
    // It also catches a TypeName annotation written in the wrong position.
    // `FAYE_ATTR(...)` on the line BEFORE `struct` is ignored by GCC with only
    // a -Wattributes warning, so the name silently falls back to the C++
    // identifier and this check fails.
    Ecs::World world;
    registerEngineComponents(world);

    int checked = 0;
    for (const Ecs::ComponentTypeInfo &info : world.types().all())
    {
        if (info.name == nullptr || info.descriptor == nullptr)
            continue;
        INFO("component: ", std::string{info.name},
             "  descriptor name: ", std::string{info.descriptor->name});
        CHECK(std::string_view{info.descriptor->name} == std::string_view{info.name});
        CHECK(info.descriptor->id == info.typeId);
        ++checked;
    }
    CHECK(checked == 8);   // every engine component is reflected
}

// ---------------------------------------------------------------------------
// 1.10 -- onFieldChanged. Nothing invokes it until the generated drawer lands
// in 1.11, so these tests call it exactly as that drawer will: write the field,
// then hand the hook the component, the field, the world and the entity.
// ---------------------------------------------------------------------------
namespace
{
    const Ecs::FieldDescriptor &fieldNamed(const Ecs::TypeDescriptor &type, std::string_view name)
    {
        for (uint32_t i = 0; i < type.fieldCount; ++i)
            if (std::string_view{type.fields[i].name} == name)
                return type.fields[i];
        FAIL("no such field: ", std::string{name});
        return type.fields[0];
    }
}

TEST_CASE("the camera hook enforces exactly one primary camera")
{
    Ecs::World world;
    registerEngineComponents(world);

    const Ecs::Entity first = world.create();
    const Ecs::Entity second = world.create();
    world.add<CameraComponent>(first).primary = true;
    world.add<CameraComponent>(second);

    const Ecs::TypeDescriptor &d = Ecs::describe<CameraComponent>();
    REQUIRE(d.onFieldChanged != nullptr);

    // What the inspector does: write the field, then notify.
    CameraComponent &promoted = *world.tryGet<CameraComponent>(second);
    promoted.primary = true;
    d.onFieldChanged(&promoted, fieldNamed(d, "primary"), world, second);

    CHECK(promoted.primary);
    CHECK_FALSE(world.tryGet<CameraComponent>(first)->primary);
}

TEST_CASE("the camera hook ignores fields that carry no invariant")
{
    Ecs::World world;
    registerEngineComponents(world);

    const Ecs::Entity first = world.create();
    const Ecs::Entity second = world.create();
    world.add<CameraComponent>(first).primary = true;
    world.add<CameraComponent>(second);

    const Ecs::TypeDescriptor &d = Ecs::describe<CameraComponent>();
    CameraComponent &other = *world.tryGet<CameraComponent>(second);

    // `camera` is NotSerialized and has no invariant -- firing the hook for it
    // must not demote anything.
    d.onFieldChanged(&other, fieldNamed(d, "camera"), world, second);
    CHECK(world.tryGet<CameraComponent>(first)->primary);

    // Nor may demotion cascade: clearing `primary` is not a promotion.
    other.primary = false;
    d.onFieldChanged(&other, fieldNamed(d, "primary"), world, second);
    CHECK(world.tryGet<CameraComponent>(first)->primary);
}

TEST_CASE("the camera hook refuses to demote the last primary camera")
{
    // The hand-written inspector drawer used to enforce this by only ever
    // offering promotion -- it bound the checkbox to a local copy. A generic
    // drawer writes the field directly, so the rule has to live in the hook,
    // where the deserializer and scripts get it too.
    Ecs::World world;
    registerEngineComponents(world);

    const Ecs::TypeDescriptor &d = Ecs::describe<CameraComponent>();

    SUBCASE("the only camera cannot give up the role")
    {
        const Ecs::Entity only = world.create();
        CameraComponent &camera = world.add<CameraComponent>(only);
        camera.primary = true;

        camera.primary = false;   // what an unchecked checkbox writes
        d.onFieldChanged(&camera, fieldNamed(d, "primary"), world, only);

        CHECK(camera.primary);
    }

    SUBCASE("demotion is allowed once another camera holds the role")
    {
        const Ecs::Entity first = world.create();
        const Ecs::Entity second = world.create();
        world.add<CameraComponent>(first).primary = true;
        CameraComponent &other = world.add<CameraComponent>(second);
        other.primary = true;
        d.onFieldChanged(&other, fieldNamed(d, "primary"), world, second);
        REQUIRE_FALSE(world.tryGet<CameraComponent>(first)->primary);

        // `first` is no longer primary, so `second` giving it up would leave
        // the scene with none -- refused for the same reason.
        other.primary = false;
        d.onFieldChanged(&other, fieldNamed(d, "primary"), world, second);
        CHECK(other.primary);

        // Promote `first` again, and now `second` may step down freely.
        CameraComponent &restored = *world.tryGet<CameraComponent>(first);
        restored.primary = true;
        d.onFieldChanged(&restored, fieldNamed(d, "primary"), world, first);
        REQUIRE_FALSE(other.primary);
    }
}

TEST_CASE("types without an invariant have no hook")
{
    // Water and Transform use Range/Radians annotations instead: those are
    // widget hints, not invariants, and must never reach a callback.
    CHECK(Ecs::describe<TransformComponent>().onFieldChanged == nullptr);
    CHECK(Ecs::describe<WaterComponent>().onFieldChanged == nullptr);
    CHECK(Ecs::describe<PointLightComponent>().onFieldChanged == nullptr);
}

#endif   // FAYE_HAS_REFLECTION
