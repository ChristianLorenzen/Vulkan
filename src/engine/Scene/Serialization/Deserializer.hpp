#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include <glm/glm.hpp>
#include <yaml-cpp/yaml.h>

#include "engine/Assets/AssetId.hpp"
#include "Core/Handles/MaterialHandle.hpp"
#include "Core/Handles/ModelHandle.hpp"

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
        // The node is held BY VALUE. YAML::Node is a refcounted handle, so the
        // copy is O(1) and aliases the same data -- and it is what lets
        // forNode() hand back a self-contained reader for a nested node
        // without the caller having to keep the parent node alive.
        explicit Deserializer(const YAML::Node &node,
                              Faye::ModelRegistry *models = nullptr,
                              Faye::MaterialRegistry *materials = nullptr)
            : node(node), models(models), materials(materials) {}

        bool has(const char *name) const;

        // ---- keyed reads: `name: value` in the bound map ------------------
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

        // ---- element reads: the BOUND node is itself the value ------------
        // Mirrors the keyed set. Used on a reader obtained from forNode() for
        // one element of a sequence, where there is no key to read by.
        float readFloatValue(float fallback = 0.0f) const;
        bool readBoolValue(bool fallback = false) const;
        int32_t readIntValue(int32_t fallback = 0) const;
        uint32_t readUintValue(uint32_t fallback = 0) const;
        std::string readStringValue(const std::string &fallback = {}) const;
        glm::vec2 readVec2Value(const glm::vec2 &fallback = {}) const;
        glm::vec3 readVec3Value(const glm::vec3 &fallback = {}) const;
        glm::vec4 readVec4Value(const glm::vec4 &fallback = {}) const;
        Faye::Uuid readUuidValue(const Faye::Uuid &fallback = {}) const;
        Faye::ModelHandle readModelAssetValue() const;
        Faye::MaterialHandle readMaterialAssetValue() const;

        // ---- structure ----------------------------------------------------
        // Descending returns another Deserializer rather than a YAML::Node, so
        // a generic walker (ReflectedSerializers) never has to include
        // yaml-cpp: this class stays the only place that knows the format.

        // Reader bound to node[name]. Undefined-but-safe when the key is
        // absent -- isMapValue()/isSequenceValue() both report false.
        Deserializer child(const char *name) const;

        // Shape of the BOUND node.
        bool isMapValue() const;
        bool isSequenceValue() const;
        size_t sizeValue() const;                 // 0 unless the bound node is a sequence
        Deserializer atValue(size_t index) const; // reader over element `index`

        // Raw access to a nested node (e.g. `effects` sequence / map elements).
        YAML::Node fieldNode(const char *name) const;

        // ---- value-node parsers -------------------------------------------
        // Parse a node that IS the value. Every read above routes through
        // these, so keyed and element reads can never diverge in how they
        // handle a malformed or missing node.
        static float parseFloat(const YAML::Node &value, float fallback);
        static bool parseBool(const YAML::Node &value, bool fallback);
        static int32_t parseInt(const YAML::Node &value, int32_t fallback);
        static uint32_t parseUint(const YAML::Node &value, uint32_t fallback);
        static std::string parseString(const YAML::Node &value, const std::string &fallback);
        static glm::vec2 parseVec2(const YAML::Node &value, const glm::vec2 &fallback);
        static glm::vec3 parseVec3(const YAML::Node &value, const glm::vec3 &fallback);
        static glm::vec4 parseVec4(const YAML::Node &value, const glm::vec4 &fallback);
        static Faye::Uuid parseUuid(const YAML::Node &value, const Faye::Uuid &fallback);

        // Safe scalar reads from an arbitrary node (used by nested structures).
        static float readFloatFrom(const YAML::Node &n, const char *name, float fallback);
        static bool readBoolFrom(const YAML::Node &n, const char *name, bool fallback);
        static std::string readStringFrom(const YAML::Node &n, const char *name, const std::string &fallback);
        static glm::vec3 readVec3From(const YAML::Node &n, const char *name, const glm::vec3 &fallback);
        static glm::vec4 readVec4From(const YAML::Node &n, const char *name, const glm::vec4 &fallback);

    private:
        // Shared by the keyed and element asset reads: an asset id resolves the
        // same way regardless of how the id itself was reached.
        Faye::ModelHandle resolveModel(const Faye::Uuid &id) const;
        Faye::MaterialHandle resolveMaterial(const Faye::Uuid &id) const;

        YAML::Node node;
        Faye::ModelRegistry *models;
        Faye::MaterialRegistry *materials;
    };
}
