#include "engine/Scene/Serialization/Deserializer.hpp"
#include "engine/Assets/ModelRegistry.hpp"
#include "Renderer/Material/MaterialRegistry.hpp"

namespace Faye::Ecs
{
    // ---- value-node parsers ----------------------------------------------
    // The single implementation of "turn this node into a T, or fall back".
    // Everything else in this file is a two-line adapter onto one of these.

    float Deserializer::parseFloat(const YAML::Node &value, float fallback)
    {
        if (!value || !value.IsScalar())
            return fallback;
        try
        {
            return value.as<float>();
        }
        catch (const YAML::BadConversion &)
        {
            return fallback;
        }
    }

    bool Deserializer::parseBool(const YAML::Node &value, bool fallback)
    {
        if (!value || !value.IsScalar())
            return fallback;
        try
        {
            return value.as<bool>();
        }
        catch (const YAML::BadConversion &)
        {
            return fallback;
        }
    }

    int32_t Deserializer::parseInt(const YAML::Node &value, int32_t fallback)
    {
        if (!value || !value.IsScalar())
            return fallback;
        try
        {
            return value.as<int32_t>();
        }
        catch (const YAML::BadConversion &)
        {
            return fallback;
        }
    }

    uint32_t Deserializer::parseUint(const YAML::Node &value, uint32_t fallback)
    {
        if (!value || !value.IsScalar())
            return fallback;
        try
        {
            return value.as<uint32_t>();
        }
        catch (const YAML::BadConversion &)
        {
            return fallback;
        }
    }

    std::string Deserializer::parseString(const YAML::Node &value, const std::string &fallback)
    {
        if (!value || !value.IsScalar())
            return fallback;
        try
        {
            return value.as<std::string>();
        }
        catch (const YAML::BadConversion &)
        {
            return fallback;
        }
    }

    glm::vec2 Deserializer::parseVec2(const YAML::Node &value, const glm::vec2 &fallback)
    {
        if (!value || !value.IsSequence() || value.size() < 2)
            return fallback;
        try
        {
            return glm::vec2(value[0].as<float>(), value[1].as<float>());
        }
        catch (const YAML::BadConversion &)
        {
            return fallback;
        }
    }

    glm::vec3 Deserializer::parseVec3(const YAML::Node &value, const glm::vec3 &fallback)
    {
        if (!value || !value.IsSequence() || value.size() < 3)
            return fallback;
        try
        {
            return glm::vec3(value[0].as<float>(), value[1].as<float>(), value[2].as<float>());
        }
        catch (const YAML::BadConversion &)
        {
            return fallback;
        }
    }

    glm::vec4 Deserializer::parseVec4(const YAML::Node &value, const glm::vec4 &fallback)
    {
        if (!value || !value.IsSequence() || value.size() < 4)
            return fallback;
        try
        {
            return glm::vec4(value[0].as<float>(), value[1].as<float>(),
                             value[2].as<float>(), value[3].as<float>());
        }
        catch (const YAML::BadConversion &)
        {
            return fallback;
        }
    }

    Faye::Uuid Deserializer::parseUuid(const YAML::Node &value, const Faye::Uuid &fallback)
    {
        if (!value || !value.IsScalar())
            return fallback;
        try
        {
            return Faye::Uuid::fromString(value.as<std::string>());
        }
        catch (const std::exception &)
        {
            return fallback;
        }
    }

    // ---- keyed reads ------------------------------------------------------

    bool Deserializer::has(const char *name) const
    {
        return node[name].IsDefined();
    }

    float Deserializer::readFloat(const char *name, float fallback) const
    {
        return parseFloat(node[name], fallback);
    }

    bool Deserializer::readBool(const char *name, bool fallback) const
    {
        return parseBool(node[name], fallback);
    }

    int32_t Deserializer::readInt(const char *name, int32_t fallback) const
    {
        return parseInt(node[name], fallback);
    }

    uint32_t Deserializer::readUint(const char *name, uint32_t fallback) const
    {
        return parseUint(node[name], fallback);
    }

    std::string Deserializer::readString(const char *name, const std::string &fallback) const
    {
        return parseString(node[name], fallback);
    }

    glm::vec2 Deserializer::readVec2(const char *name, const glm::vec2 &fallback) const
    {
        return parseVec2(node[name], fallback);
    }

    glm::vec3 Deserializer::readVec3(const char *name, const glm::vec3 &fallback) const
    {
        return parseVec3(node[name], fallback);
    }

    glm::vec4 Deserializer::readVec4(const char *name, const glm::vec4 &fallback) const
    {
        return parseVec4(node[name], fallback);
    }

    Faye::Uuid Deserializer::readUuid(const char *name, const Faye::Uuid &fallback) const
    {
        return parseUuid(node[name], fallback);
    }

    Faye::ModelHandle Deserializer::readModelAsset(const char *name) const
    {
        return resolveModel(readUuid(name));
    }

    Faye::MaterialHandle Deserializer::readMaterialAsset(const char *name) const
    {
        return resolveMaterial(readUuid(name));
    }

    // ---- element reads: the bound node IS the value -----------------------

    float Deserializer::readFloatValue(float fallback) const { return parseFloat(node, fallback); }
    bool Deserializer::readBoolValue(bool fallback) const { return parseBool(node, fallback); }
    int32_t Deserializer::readIntValue(int32_t fallback) const { return parseInt(node, fallback); }
    uint32_t Deserializer::readUintValue(uint32_t fallback) const { return parseUint(node, fallback); }

    std::string Deserializer::readStringValue(const std::string &fallback) const
    {
        return parseString(node, fallback);
    }

    glm::vec2 Deserializer::readVec2Value(const glm::vec2 &fallback) const
    {
        return parseVec2(node, fallback);
    }

    glm::vec3 Deserializer::readVec3Value(const glm::vec3 &fallback) const
    {
        return parseVec3(node, fallback);
    }

    glm::vec4 Deserializer::readVec4Value(const glm::vec4 &fallback) const
    {
        return parseVec4(node, fallback);
    }

    Faye::Uuid Deserializer::readUuidValue(const Faye::Uuid &fallback) const
    {
        return parseUuid(node, fallback);
    }

    Faye::ModelHandle Deserializer::readModelAssetValue() const
    {
        return resolveModel(readUuidValue());
    }

    Faye::MaterialHandle Deserializer::readMaterialAssetValue() const
    {
        return resolveMaterial(readUuidValue());
    }

    // ---- structure --------------------------------------------------------

    Deserializer Deserializer::child(const char *name) const
    {
        return Deserializer{node[name], models, materials};
    }

    bool Deserializer::isMapValue() const
    {
        return node && node.IsMap();
    }

    bool Deserializer::isSequenceValue() const
    {
        return node && node.IsSequence();
    }

    size_t Deserializer::sizeValue() const
    {
        return isSequenceValue() ? node.size() : 0;
    }

    Deserializer Deserializer::atValue(size_t index) const
    {
        return Deserializer{node[index], models, materials};
    }

    YAML::Node Deserializer::fieldNode(const char *name) const
    {
        return node[name];
    }

    Faye::ModelHandle Deserializer::resolveModel(const Faye::Uuid &id) const
    {
        if (id.isNull() || models == nullptr)
            return Faye::ModelHandle{};
        if (const auto handle = models->findByAssetId(id))
            return *handle;
        return Faye::ModelHandle{};
    }

    Faye::MaterialHandle Deserializer::resolveMaterial(const Faye::Uuid &id) const
    {
        if (id.isNull() || materials == nullptr)
            return Faye::MaterialHandle{};
        if (const auto handle = materials->findByAssetId(id))
            return *handle;
        return Faye::MaterialHandle{};
    }

    // ---- legacy node+name helpers (nested structures) ---------------------

    float Deserializer::readFloatFrom(const YAML::Node &n, const char *name, float fallback)
    {
        return parseFloat(n[name], fallback);
    }

    bool Deserializer::readBoolFrom(const YAML::Node &n, const char *name, bool fallback)
    {
        return parseBool(n[name], fallback);
    }

    std::string Deserializer::readStringFrom(const YAML::Node &n, const char *name, const std::string &fallback)
    {
        return parseString(n[name], fallback);
    }

    glm::vec3 Deserializer::readVec3From(const YAML::Node &n, const char *name, const glm::vec3 &fallback)
    {
        return parseVec3(n[name], fallback);
    }

    glm::vec4 Deserializer::readVec4From(const YAML::Node &n, const char *name, const glm::vec4 &fallback)
    {
        return parseVec4(n[name], fallback);
    }
}
