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
    // Matches the forward-declared `Serializer` in Core/ECS/World.hpp (the
    // reserved ComponentTypeInfo::serialize slot). Wraps a YAML::Emitter and
    // writes one component's fields as a map. Registry pointers are nullable:
    // headless tests pass nullptr; the scene writer passes the live registries
    // (needed only by writeAssetField to resolve handles -> asset ids).
    class Serializer
    {
    public:
        explicit Serializer(YAML::Emitter &emitter,
                            const Faye::ModelRegistry *models = nullptr,
                            const Faye::MaterialRegistry *materials = nullptr)
            : emitter(emitter), models(models), materials(materials) {}

        void writeField(const char *name, float value);
        void writeField(const char *name, bool value);
        void writeField(const char *name, int32_t value);
        void writeField(const char *name, uint32_t value);
        void writeField(const char *name, const std::string &value);
        void writeField(const char *name, const glm::vec2 &value);
        void writeField(const char *name, const glm::vec3 &value);
        void writeField(const char *name, const glm::vec4 &value);
        void writeField(const char *name, const Faye::Uuid &value);

        // Registry-local handles are written as deterministic asset id strings
        // (null uuid when the handle is invalid or the registry is absent).
        void writeAssetField(const char *name, Faye::ModelHandle handle);
        void writeAssetField(const char *name, Faye::MaterialHandle handle);

        // Bare values, no key: the elements of a sequence. Mirrors the
        // writeField set above one-for-one.
        //
        // Needed because writeField always emits `key: value`, so before this
        // the only serialisable sequence element was a map (see
        // serializePostProcessStack). A reflected vector<float> or
        // vector<std::string> had no way to write itself.
        void writeElement(bool value);
        void writeElement(int32_t value);
        void writeElement(uint32_t value);
        void writeElement(float value);
        void writeElement(const std::string &value);
        void writeElement(const glm::vec2 &value);
        void writeElement(const glm::vec3 &value);
        void writeElement(const glm::vec4 &value);
        void writeElement(const Faye::Uuid &value);
        void writeAssetElement(Faye::ModelHandle handle);
        void writeAssetElement(Faye::MaterialHandle handle);

        // Nested structures (e.g. post-process effects[]).
        void beginFieldMap(const char *name);     // <key>: { ...
        void beginMap();                          // bare { (sequence element)
        void endMap();                            // }
        void beginFieldSequence(const char *name); // <key>: [ ...
        void beginSequence();                      // bare [ (nested sequence element)
        void endSequence();                        // ]

    private:
        YAML::Emitter &emitter;
        const Faye::ModelRegistry *models;
        const Faye::MaterialRegistry *materials;
    };
}
