#include <doctest/doctest.h>

#include <yaml-cpp/yaml.h>

#include "Core/ECS/World.hpp"
#include "Renderer/PostProcess/PostProcessEffectLibrary.hpp"
#include "Scene/Entities/Components.hpp"
#include "Scene/Entities/RegisterComponents.hpp"
#include "Scene/Serialization/ComponentSerializers.hpp"
#include "Scene/Serialization/Deserializer.hpp"
#include "Scene/Serialization/Serializer.hpp"
#include "Scripting/LuaScriptSystem.hpp"
#include "Scripting/ScriptComponents.hpp"

using namespace Faye;

namespace
{
    // Serialize `src` through its thunk into a YAML map, then deserialize back
    // into `dst` through the matching thunk.
    template <class SerializeFn, class DeserializeFn, class T>
    void roundTrip(const T &src, T &dst, SerializeFn serialize, DeserializeFn deserialize)
    {
        YAML::Emitter emitter;
        Ecs::Serializer serializer(emitter);   // null registries (headless)
        emitter << YAML::BeginMap;
        serialize(&src, serializer);
        emitter << YAML::EndMap;

        const YAML::Node node = YAML::Load(emitter.c_str());
        REQUIRE(node.IsMap());

        Ecs::Deserializer deserializer(node);
        deserialize(&dst, deserializer);
    }
}

TEST_CASE("transform round-trips through YAML")
{
    TransformComponent src;
    src.translation = {1.0f, 2.0f, 3.0f};
    src.rotation = {0.5f, 0.0f, -1.25f};   // radians
    src.scale = {2.0f, 2.0f, 2.0f};

    TransformComponent dst;
    roundTrip(src, dst, Ecs::serializeTransform, Ecs::deserializeTransform);

    CHECK(dst.translation == src.translation);
    CHECK(dst.rotation == src.rotation);
    CHECK(dst.scale == src.scale);
}

TEST_CASE("lights and water round-trip through YAML")
{
    PointLightComponent point;
    point.color = {1.0f, 0.5f, 0.25f};
    point.intensity = 4.5f;
    point.radius = 2.0f;
    PointLightComponent pointDst;
    roundTrip(point, pointDst, Ecs::serializePointLight, Ecs::deserializePointLight);
    CHECK(pointDst.color == point.color);
    CHECK(pointDst.intensity == doctest::Approx(point.intensity));
    CHECK(pointDst.radius == doctest::Approx(point.radius));

    DirectionalLightComponent sun;
    sun.color = {1.0f, 0.96f, 0.9f};
    sun.intensity = 0.2f;
    DirectionalLightComponent sunDst;
    roundTrip(sun, sunDst, Ecs::serializeDirectionalLight, Ecs::deserializeDirectionalLight);
    CHECK(sunDst.color == sun.color);
    CHECK(sunDst.intensity == doctest::Approx(sun.intensity));

    WaterComponent water;
    water.subdivisions = 128;
    WaterComponent waterDst;
    roundTrip(water, waterDst, Ecs::serializeWater, Ecs::deserializeWater);
    CHECK(waterDst.subdivisions == water.subdivisions);

    RigidBody2dComponent body;
    body.velocity = {3.0f, -1.0f};
    body.mass = 7.0f;
    RigidBody2dComponent bodyDst;
    roundTrip(body, bodyDst, Ecs::serializeRigidBody2d, Ecs::deserializeRigidBody2d);
    CHECK(bodyDst.velocity == body.velocity);
    CHECK(bodyDst.mass == doctest::Approx(body.mass));
}

TEST_CASE("camera primary flag round-trips")
{
    CameraComponent src;
    src.primary = true;
    CameraComponent dst;
    roundTrip(src, dst, Ecs::serializeCamera, Ecs::deserializeCamera);
    CHECK(dst.primary);
}

TEST_CASE("post-process stack with effects round-trips")
{
    PostProcessStackComponent src;
    src.enabled = true;
    PostProcessEffectComponent effect;
    effect.definitionId = "tone_map";
    effect.enabled = true;
    effect.parameters.color = {1.0f, 0.5f, 0.25f, 1.0f};
    effect.parameters.params = {0.1f, 0.2f, 0.3f, 0.4f};
    src.effects.push_back(effect);

    PostProcessStackComponent dst;
    roundTrip(src, dst, Ecs::serializePostProcessStack, Ecs::deserializePostProcessStack);

    CHECK(dst.enabled == src.enabled);
    REQUIRE(dst.effects.size() == 1);
    CHECK(dst.effects[0].definitionId == "tone_map");
    CHECK(dst.effects[0].enabled);
    CHECK(dst.effects[0].parameters.color == effect.parameters.color);
    CHECK(dst.effects[0].parameters.params == effect.parameters.params);
}

TEST_CASE("missing fields keep component defaults")
{
    // Serialize only the non-default fields: a deserialize of a node with no
    // fields must leave the default-constructed component untouched.
    YAML::Node node = YAML::Load("scale: [5, 5, 5]");

    TransformComponent dst;   // defaults
    Ecs::Deserializer deserializer(node);
    Ecs::deserializeTransform(&dst, deserializer);

    CHECK(dst.translation == glm::vec3(0.0f));
    CHECK(dst.rotation == glm::vec3(0.0f));
    CHECK(dst.scale == glm::vec3(5.0f));
}

TEST_CASE("mesh assets serialize as null uuid without registries")
{
    MeshRendererComponent src;   // invalid handles
    YAML::Emitter emitter;
    Ecs::Serializer serializer(emitter);   // null registries
    emitter << YAML::BeginMap;
    Ecs::serializeMesh(&src, serializer);
    emitter << YAML::EndMap;

    const YAML::Node node = YAML::Load(emitter.c_str());
    CHECK(node["modelHandle"].as<std::string>() == "00000000-0000-0000-0000-000000000000");
    CHECK(node["materialHandle"].as<std::string>() == "00000000-0000-0000-0000-000000000000");

    MeshRendererComponent dst;
    Ecs::Deserializer deserializer(node);
    Ecs::deserializeMesh(&dst, deserializer);
    CHECK_FALSE(dst.modelHandle.isValid());
    CHECK_FALSE(dst.materialHandle.isValid());
}

TEST_CASE("script components round-trip their paths")
{
    LuaScriptComponent luaSrc;
    luaSrc.scriptPath = "src/Scripting/ExampleScripts/rotator.lua";
    LuaScriptComponent luaDst;
    roundTrip(luaSrc, luaDst, Ecs::serializeLuaScript, Ecs::deserializeLuaScript);
    CHECK(luaDst.scriptPath == luaSrc.scriptPath);

    NativeScriptComponent nativeSrc;
    nativeSrc.scriptPath = "bin/libfaye_rotator_script.so";
    nativeSrc.scriptName = "Rotator";
    NativeScriptComponent nativeDst;
    roundTrip(nativeSrc, nativeDst, Ecs::serializeNativeScript, Ecs::deserializeNativeScript);
    CHECK(nativeDst.scriptPath == nativeSrc.scriptPath);
    CHECK(nativeDst.scriptName == nativeSrc.scriptName);
}

TEST_CASE("registerEngineComponents stamps the serialize/deserialize slots")
{
    Ecs::World world;
    registerEngineComponents(world);

    for (const Ecs::ComponentTypeInfo &info : world.types().all())
    {
        if (info.name == nullptr)
            continue;
        CHECK(info.serialize != nullptr);
        CHECK(info.deserialize != nullptr);
    }
}
