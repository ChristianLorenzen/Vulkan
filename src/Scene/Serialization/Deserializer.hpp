#pragma once

#include <cstdint>
#include <string>

#include <glm/glm.hpp>
#include <yaml-cpp/yaml.h>

#include "Assets/AssetId.hpp"
#include "Assets/ModelHandle.hpp"
#include "Renderer/Material/MaterialHandle.hpp"

namespace Faye
{
    class ModelRegistry;
    class MaterialRegistry;
}

namespace Faye::Ecs
{
    // Matches the forward-declared `Deserializer` in Core/ECS/World.hpp (the
    // reserved ComponentTypeInfo::deserialize slot). Wraps a YAML::Node and
    // reads one component's fields into a default-constructed component.
    // Registry pointers are nullable for headless tests; the scene reader
    // passes the live registries (needed by readModelAsset/readMaterialAsset
    // to resolve asset id strings -> registry-local handles).
    class Deserializer
    {
    public:
        explicit Deserializer(const YAML::Node &node,
                              Faye::ModelRegistry *models = nullptr,
                              Faye::MaterialRegistry *materials = nullptr)
            : node(node), models(models), materials(materials) {}

        bool has(const char *name) const;

        float readFloat(const char *name, float fallback = 0.0f) const;
        bool readBool(const char *name, bool fallback = false) const;
        int32_t readInt(const char *name, int32_t fallback = 0) const;
        uint32_t readUint(const char *name, uint32_t fallback = 0) const;
        std::string readString(const char *name, const std::string &fallback = {}) const;
        glm::vec2 readVec2(const char *name, const glm::vec2 &fallback = {}) const;
        glm::vec3 readVec3(const char *name, const glm::vec3 &fallback = {}) const;
        glm::vec4 readVec4(const char *name, const glm::vec4 &fallback = {}) const;
        Faye::Uuid readUuid(const char *name, const Faye::Uuid &fallback = {}) const;

        // Asset id strings -> registry-local handles (null handle when the id
        // is null, not found, or the registry is absent).
        Faye::ModelHandle readModelAsset(const char *name) const;
        Faye::MaterialHandle readMaterialAsset(const char *name) const;

        // Raw access to a nested node (e.g. `effects` sequence / map elements).
        YAML::Node fieldNode(const char *name) const;

        // Safe scalar reads from an arbitrary node (used by nested structures).
        static float readFloatFrom(const YAML::Node &n, const char *name, float fallback);
        static bool readBoolFrom(const YAML::Node &n, const char *name, bool fallback);
        static std::string readStringFrom(const YAML::Node &n, const char *name, const std::string &fallback);
        static glm::vec3 readVec3From(const YAML::Node &n, const char *name, const glm::vec3 &fallback);
        static glm::vec4 readVec4From(const YAML::Node &n, const char *name, const glm::vec4 &fallback);

    private:
        const YAML::Node &node;
        Faye::ModelRegistry *models;
        Faye::MaterialRegistry *materials;
    };
}
