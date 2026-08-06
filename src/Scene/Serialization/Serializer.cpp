#include "Scene/Serialization/Serializer.hpp"

#include "Assets/ModelRegistry.hpp"
#include "Renderer/Material/MaterialRegistry.hpp"

namespace Faye::Ecs
{
    void Serializer::writeField(const char *name, float value)
    {
        emitter << YAML::Key << name << YAML::Value << value;
    }

    void Serializer::writeField(const char *name, bool value)
    {
        emitter << YAML::Key << name << YAML::Value << value;
    }

    void Serializer::writeField(const char *name, int32_t value)
    {
        emitter << YAML::Key << name << YAML::Value << value;
    }

    void Serializer::writeField(const char *name, uint32_t value)
    {
        emitter << YAML::Key << name << YAML::Value << value;
    }

    void Serializer::writeField(const char *name, const std::string &value)
    {
        emitter << YAML::Key << name << YAML::Value << value;
    }

    void Serializer::writeField(const char *name, const glm::vec2 &value)
    {
        emitter << YAML::Key << name << YAML::Value << YAML::Flow << YAML::BeginSeq
                << value.x << value.y << YAML::EndSeq;
    }

    void Serializer::writeField(const char *name, const glm::vec3 &value)
    {
        emitter << YAML::Key << name << YAML::Value << YAML::Flow << YAML::BeginSeq
                << value.x << value.y << value.z << YAML::EndSeq;
    }

    void Serializer::writeField(const char *name, const glm::vec4 &value)
    {
        emitter << YAML::Key << name << YAML::Value << YAML::Flow << YAML::BeginSeq
                << value.x << value.y << value.z << value.w << YAML::EndSeq;
    }

    void Serializer::writeField(const char *name, const Faye::Uuid &value)
    {
        emitter << YAML::Key << name << YAML::Value << value.toString();
    }

    void Serializer::writeAssetField(const char *name, Faye::ModelHandle handle)
    {
        Faye::Uuid id;
        if (models != nullptr && handle.isValid())
        {
            if (const auto assetId = models->assetIdOf(handle))
                id = *assetId;
        }
        writeField(name, id);
    }

    void Serializer::writeAssetField(const char *name, Faye::MaterialHandle handle)
    {
        Faye::Uuid id;
        if (materials != nullptr && handle.isValid())
        {
            if (const auto assetId = materials->assetIdOf(handle))
                id = *assetId;
        }
        writeField(name, id);
    }

    // ---- Bare sequence elements ------------------------------------------
    // Same emitted representation as the matching writeField, minus the key.

    void Serializer::writeElement(bool value) { emitter << value; }
    void Serializer::writeElement(int32_t value) { emitter << value; }
    void Serializer::writeElement(uint32_t value) { emitter << value; }
    void Serializer::writeElement(float value) { emitter << value; }
    void Serializer::writeElement(const std::string &value) { emitter << value; }
    void Serializer::writeElement(const Faye::Uuid &value) { emitter << value.toString(); }

    void Serializer::writeElement(const glm::vec2 &value)
    {
        emitter << YAML::Flow << YAML::BeginSeq << value.x << value.y << YAML::EndSeq;
    }

    void Serializer::writeElement(const glm::vec3 &value)
    {
        emitter << YAML::Flow << YAML::BeginSeq << value.x << value.y << value.z << YAML::EndSeq;
    }

    void Serializer::writeElement(const glm::vec4 &value)
    {
        emitter << YAML::Flow << YAML::BeginSeq
                << value.x << value.y << value.z << value.w << YAML::EndSeq;
    }

    void Serializer::writeAssetElement(Faye::ModelHandle handle)
    {
        Faye::Uuid id;
        if (models != nullptr && handle.isValid())
        {
            if (const auto assetId = models->assetIdOf(handle))
                id = *assetId;
        }
        writeElement(id);
    }

    void Serializer::writeAssetElement(Faye::MaterialHandle handle)
    {
        Faye::Uuid id;
        if (materials != nullptr && handle.isValid())
        {
            if (const auto assetId = materials->assetIdOf(handle))
                id = *assetId;
        }
        writeElement(id);
    }

    void Serializer::beginFieldMap(const char *name)
    {
        emitter << YAML::Key << name << YAML::Value << YAML::BeginMap;
    }

    void Serializer::beginMap()
    {
        emitter << YAML::BeginMap;
    }

    void Serializer::endMap()
    {
        emitter << YAML::EndMap;
    }

    void Serializer::beginFieldSequence(const char *name)
    {
        emitter << YAML::Key << name << YAML::Value << YAML::BeginSeq;
    }

    void Serializer::beginSequence()
    {
        emitter << YAML::BeginSeq;
    }

    void Serializer::endSequence()
    {
        emitter << YAML::EndSeq;
    }
}
