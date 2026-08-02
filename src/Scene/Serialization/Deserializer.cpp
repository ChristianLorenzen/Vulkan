#include "Scene/Serialization/Deserializer.hpp"

#include "Assets/ModelRegistry.hpp"
#include "Renderer/Material/MaterialRegistry.hpp"

namespace Faye::Ecs
{
    bool Deserializer::has(const char *name) const
    {
        return node[name].IsDefined();
    }

    float Deserializer::readFloat(const char *name, float fallback) const
    {
        return readFloatFrom(node, name, fallback);
    }

    bool Deserializer::readBool(const char *name, bool fallback) const
    {
        return readBoolFrom(node, name, fallback);
    }

    int32_t Deserializer::readInt(const char *name, int32_t fallback) const
    {
        const YAML::Node field = node[name];
        if (!field || !field.IsScalar())
            return fallback;
        try
        {
            return field.as<int32_t>();
        }
        catch (const YAML::BadConversion &)
        {
            return fallback;
        }
    }

    uint32_t Deserializer::readUint(const char *name, uint32_t fallback) const
    {
        const YAML::Node field = node[name];
        if (!field || !field.IsScalar())
            return fallback;
        try
        {
            return field.as<uint32_t>();
        }
        catch (const YAML::BadConversion &)
        {
            return fallback;
        }
    }

    std::string Deserializer::readString(const char *name, const std::string &fallback) const
    {
        return readStringFrom(node, name, fallback);
    }

    glm::vec2 Deserializer::readVec2(const char *name, const glm::vec2 &fallback) const
    {
        const YAML::Node field = node[name];
        if (!field || !field.IsSequence() || field.size() < 2)
            return fallback;
        try
        {
            return glm::vec2(field[0].as<float>(), field[1].as<float>());
        }
        catch (const YAML::BadConversion &)
        {
            return fallback;
        }
    }

    glm::vec3 Deserializer::readVec3(const char *name, const glm::vec3 &fallback) const
    {
        const YAML::Node field = node[name];
        if (!field || !field.IsSequence() || field.size() < 3)
            return fallback;
        try
        {
            return glm::vec3(field[0].as<float>(), field[1].as<float>(), field[2].as<float>());
        }
        catch (const YAML::BadConversion &)
        {
            return fallback;
        }
    }

    glm::vec4 Deserializer::readVec4(const char *name, const glm::vec4 &fallback) const
    {
        return readVec4From(node, name, fallback);
    }

    Faye::Uuid Deserializer::readUuid(const char *name, const Faye::Uuid &fallback) const
    {
        const YAML::Node field = node[name];
        if (!field || !field.IsScalar())
            return fallback;
        try
        {
            return Faye::Uuid::fromString(field.as<std::string>());
        }
        catch (const std::exception &)
        {
            return fallback;
        }
    }

    Faye::ModelHandle Deserializer::readModelAsset(const char *name) const
    {
        const Faye::Uuid id = readUuid(name);
        if (id.isNull() || models == nullptr)
            return Faye::ModelHandle{};
        if (const auto handle = models->findByAssetId(id))
            return *handle;
        return Faye::ModelHandle{};
    }

    Faye::MaterialHandle Deserializer::readMaterialAsset(const char *name) const
    {
        const Faye::Uuid id = readUuid(name);
        if (id.isNull() || materials == nullptr)
            return Faye::MaterialHandle{};
        if (const auto handle = materials->findByAssetId(id))
            return *handle;
        return Faye::MaterialHandle{};
    }

    YAML::Node Deserializer::fieldNode(const char *name) const
    {
        return node[name];
    }

    float Deserializer::readFloatFrom(const YAML::Node &n, const char *name, float fallback)
    {
        const YAML::Node field = n[name];
        if (!field || !field.IsScalar())
            return fallback;
        try
        {
            return field.as<float>();
        }
        catch (const YAML::BadConversion &)
        {
            return fallback;
        }
    }

    bool Deserializer::readBoolFrom(const YAML::Node &n, const char *name, bool fallback)
    {
        const YAML::Node field = n[name];
        if (!field || !field.IsScalar())
            return fallback;
        try
        {
            return field.as<bool>();
        }
        catch (const YAML::BadConversion &)
        {
            return fallback;
        }
    }

    std::string Deserializer::readStringFrom(const YAML::Node &n, const char *name, const std::string &fallback)
    {
        const YAML::Node field = n[name];
        if (!field || !field.IsScalar())
            return fallback;
        try
        {
            return field.as<std::string>();
        }
        catch (const YAML::BadConversion &)
        {
            return fallback;
        }
    }

    glm::vec3 Deserializer::readVec3From(const YAML::Node &n, const char *name, const glm::vec3 &fallback)
    {
        const YAML::Node field = n[name];
        if (!field || !field.IsSequence() || field.size() < 3)
            return fallback;
        try
        {
            return glm::vec3(field[0].as<float>(), field[1].as<float>(), field[2].as<float>());
        }
        catch (const YAML::BadConversion &)
        {
            return fallback;
        }
    }

    glm::vec4 Deserializer::readVec4From(const YAML::Node &n, const char *name, const glm::vec4 &fallback)
    {
        const YAML::Node field = n[name];
        if (!field || !field.IsSequence() || field.size() < 4)
            return fallback;
        try
        {
            return glm::vec4(field[0].as<float>(), field[1].as<float>(), field[2].as<float>(), field[3].as<float>());
        }
        catch (const YAML::BadConversion &)
        {
            return fallback;
        }
    }
}
