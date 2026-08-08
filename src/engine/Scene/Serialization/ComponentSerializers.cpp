#include "engine/Scene/Serialization/ComponentSerializers.hpp"
#include "engine/Scene/Serialization/Deserializer.hpp"
#include "engine/Scene/Serialization/Serializer.hpp"
#include "engine/Scene/Entities/Components.hpp"
#include "Renderer/PostProcess/PostProcessEffectLibrary.hpp"
#include "engine/Scripting/LuaScriptSystem.hpp"
#include "engine/Scripting/ScriptComponents.hpp"
namespace Faye::Ecs
{
    // ---- Transform -------------------------------------------------------
    namespace
    {
        // Exactly the table Describe.hpp now generates from enumerators_of, and
        // exactly the boilerplate step 1.12 exists to delete -- it is here only
        // because the reflection-off build has no other way to persist an enum,
        // and because the parity harness compares against hand-written code by
        // definition. Names must match the C++ identifiers: that is what the
        // reflected writer emits.
        const char *testEnumName(TestEnum value)
        {
            switch (value)
            {
            case TESTONE: return "TESTONE";
            case TESTTWO: return "TESTTWO";
            case FINAL:   return "FINAL";
            }
            return "TESTONE";
        }

        TestEnum testEnumFromName(const std::string &name, TestEnum fallback)
        {
            if (name == "TESTONE") return TESTONE;
            if (name == "TESTTWO") return TESTTWO;
            if (name == "FINAL")   return FINAL;
            return fallback;
        }
    }

    void serializeTransform(const void *raw, Serializer &s)
    {
        const auto &c = *static_cast<const TransformComponent *>(raw);
        s.writeField("translation", c.translation);
        s.writeField("scale", c.scale);
        s.writeField("rotation", c.rotation);   // radians
        s.writeField("enumVal", std::string{testEnumName(c.enumVal)});
    }

    void deserializeTransform(void *raw, Deserializer &d)
    {
        auto &c = *static_cast<TransformComponent *>(raw);
        c.translation = d.readVec3("translation", c.translation);
        c.rotation = d.readVec3("rotation", c.rotation);
        c.scale = d.readVec3("scale", c.scale);
        c.enumVal = testEnumFromName(d.readString("enumVal", testEnumName(c.enumVal)), c.enumVal);
    }

    // ---- Mesh (asset id references) --------------------------------------
    void serializeMesh(const void *raw, Serializer &s)
    {
        const auto &c = *static_cast<const MeshRendererComponent *>(raw);
        s.writeAssetField("modelHandle", c.modelHandle);
        s.writeAssetField("materialHandle", c.materialHandle);
        s.writeField("view", c.view);
    }

    void deserializeMesh(void *raw, Deserializer &d)
    {
        auto &c = *static_cast<MeshRendererComponent *>(raw);
        c.modelHandle = d.readModelAsset("modelHandle");
        c.materialHandle = d.readMaterialAsset("materialHandle");
        c.view = d.readBool("view", c.view);
    }

    // ---- Camera (internals are runtime-only) -----------------------------
    void serializeCamera(const void *raw, Serializer &s)
    {
        const auto &c = *static_cast<const CameraComponent *>(raw);
        s.writeField("primary", c.primary);
    }

    void deserializeCamera(void *raw, Deserializer &d)
    {
        auto &c = *static_cast<CameraComponent *>(raw);
        c.primary = d.readBool("primary", c.primary);
    }

    // ---- Water -----------------------------------------------------------
    void serializeWater(const void *raw, Serializer &s)
    {
        const auto &c = *static_cast<const WaterComponent *>(raw);
        s.writeField("subdivisions", c.subdivisions);
    }

    void deserializeWater(void *raw, Deserializer &d)
    {
        auto &c = *static_cast<WaterComponent *>(raw);
        c.subdivisions = d.readUint("subdivisions", c.subdivisions);
    }

    // ---- Point Light -----------------------------------------------------
    void serializePointLight(const void *raw, Serializer &s)
    {
        const auto &c = *static_cast<const PointLightComponent *>(raw);
        s.writeField("color", c.color);
        s.writeField("intensity", c.intensity);
        s.writeField("radius", c.radius);
    }

    void deserializePointLight(void *raw, Deserializer &d)
    {
        auto &c = *static_cast<PointLightComponent *>(raw);
        c.color = d.readVec3("color", c.color);
        c.intensity = d.readFloat("intensity", c.intensity);
        c.radius = d.readFloat("radius", c.radius);
    }

    // ---- Directional Light ------------------------------------------------
    void serializeDirectionalLight(const void *raw, Serializer &s)
    {
        const auto &c = *static_cast<const DirectionalLightComponent *>(raw);
        s.writeField("color", c.color);
        s.writeField("intensity", c.intensity);
    }

    void deserializeDirectionalLight(void *raw, Deserializer &d)
    {
        auto &c = *static_cast<DirectionalLightComponent *>(raw);
        c.color = d.readVec3("color", c.color);
        c.intensity = d.readFloat("intensity", c.intensity);
    }

    // ---- Post Process Stack (nested + vector) -----------------------------
    void serializePostProcessStack(const void *raw, Serializer &s)
    {
        const auto &c = *static_cast<const PostProcessStackComponent *>(raw);
        s.writeField("enabled", c.enabled);
        s.beginFieldSequence("effects");
        for (const PostProcessEffectComponent &effect : c.effects)
        {
            s.beginMap();
            s.writeField("definitionId", effect.definitionId);
            s.writeField("enabled", effect.enabled);
            s.beginFieldMap("parameters");
            s.writeField("color", effect.parameters.color);
            s.writeField("params", effect.parameters.params);
            s.endMap();   // parameters
            s.endMap();   // effect
        }
        s.endSequence();
    }

    void deserializePostProcessStack(void *raw, Deserializer &d)
    {
        auto &c = *static_cast<PostProcessStackComponent *>(raw);
        c.enabled = d.readBool("enabled", c.enabled);

        const YAML::Node effects = d.fieldNode("effects");
        c.effects.clear();
        if (effects && effects.IsSequence())
        {
            for (const YAML::Node &effectNode : effects)
            {
                if (!effectNode.IsMap())
                    continue;
                PostProcessEffectComponent effect;
                effect.definitionId = Deserializer::readStringFrom(effectNode, "definitionId", effect.definitionId);
                effect.enabled = Deserializer::readBoolFrom(effectNode, "enabled", effect.enabled);
                const YAML::Node params = effectNode["parameters"];
                if (params && params.IsMap())
                {
                    effect.parameters.color = Deserializer::readVec4From(params, "color", effect.parameters.color);
                    effect.parameters.params = Deserializer::readVec4From(params, "params", effect.parameters.params);
                }
                c.effects.push_back(std::move(effect));
            }
        }
    }

    // ---- RigidBody2D ------------------------------------------------------
    void serializeRigidBody2d(const void *raw, Serializer &s)
    {
        const auto &c = *static_cast<const RigidBody2dComponent *>(raw);
        s.writeField("velocity", c.velocity);
        s.writeField("mass", c.mass);
    }

    void deserializeRigidBody2d(void *raw, Deserializer &d)
    {
        auto &c = *static_cast<RigidBody2dComponent *>(raw);
        c.velocity = d.readVec2("velocity", c.velocity);
        c.mass = d.readFloat("mass", c.mass);
    }

    // ---- Lua Script (path only; re-attached by the scene reader) -----------
    void serializeLuaScript(const void *raw, Serializer &s)
    {
        const auto &c = *static_cast<const LuaScriptComponent *>(raw);
        s.writeField("scriptPath", c.scriptPath);
    }

    void deserializeLuaScript(void *raw, Deserializer &d)
    {
        auto &c = *static_cast<LuaScriptComponent *>(raw);
        c.scriptPath = d.readString("scriptPath", c.scriptPath);
    }

    // ---- Native Script (paths only; re-attached by the scene reader) -------
    void serializeNativeScript(const void *raw, Serializer &s)
    {
        const auto &c = *static_cast<const NativeScriptComponent *>(raw);
        s.writeField("scriptPath", c.scriptPath);
        s.writeField("scriptName", c.scriptName);
    }

    void deserializeNativeScript(void *raw, Deserializer &d)
    {
        auto &c = *static_cast<NativeScriptComponent *>(raw);
        c.scriptPath = d.readString("scriptPath", c.scriptPath);
        c.scriptName = d.readString("scriptName", c.scriptName);
    }
}
