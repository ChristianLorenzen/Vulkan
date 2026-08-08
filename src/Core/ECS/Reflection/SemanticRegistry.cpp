#include "Core/ECS/Reflection/SemanticRegistry.hpp"

#include <glm/glm.hpp>

#include "Core/Handles/MaterialHandle.hpp"
#include "Core/Handles/ModelHandle.hpp"
#include "Core/Serialization/Uuid.hpp"
#include "engine/Scene/Serialization/Deserializer.hpp"
#include "engine/Scene/Serialization/Serializer.hpp"
// NOTE ON LAYERING: this is the one file under Core/ECS/Reflection/ that
// includes Scene/Serialization (and therefore yaml-cpp). The ground rule is
// that the reflection HEADERS stay headless -- SemanticRegistry.hpp forward
// declares Serializer/Deserializer through TypeDescriptor.hpp and pulls in
// neither. Keeping the built-in registrations here rather than in a separate
// Scene-side file is what makes "add a type" a single-file edit.

namespace Faye::Ecs
{
    namespace
    {
        // ------------------------------------------------------------------
        // The irreducible part. Serializer's write side is an overload set, so
        // it dispatches on type for free; Deserializer's read side is NAMED per
        // type (readFloat/readVec3/...), so it needs this adapter to become one.
        // Everything below this pair is generic.
        //
        // To add a semantic: one writeValue, one readValue, one line in
        // registerBuiltins. Nothing else in the engine changes.
        // ------------------------------------------------------------------
        void writeValue(Serializer &s, const char *n, bool v)               { s.writeField(n, v); }
        void writeValue(Serializer &s, const char *n, int32_t v)            { s.writeField(n, v); }
        void writeValue(Serializer &s, const char *n, uint32_t v)           { s.writeField(n, v); }
        void writeValue(Serializer &s, const char *n, float v)              { s.writeField(n, v); }
        void writeValue(Serializer &s, const char *n, const std::string &v) { s.writeField(n, v); }
        void writeValue(Serializer &s, const char *n, const glm::vec2 &v)   { s.writeField(n, v); }
        void writeValue(Serializer &s, const char *n, const glm::vec3 &v)   { s.writeField(n, v); }
        void writeValue(Serializer &s, const char *n, const glm::vec4 &v)   { s.writeField(n, v); }
        void writeValue(Serializer &s, const char *n, const Uuid &v)        { s.writeField(n, v); }
        void writeValue(Serializer &s, const char *n, ModelHandle v)        { s.writeAssetField(n, v); }
        void writeValue(Serializer &s, const char *n, MaterialHandle v)     { s.writeAssetField(n, v); }

        // The fallback argument is the field's CURRENT value, so a key missing
        // from the file leaves the default in place. The hand-written thunks
        // already behave this way (c.radius = d.readFloat("radius", c.radius));
        // 1.7's byte-identical check depends on it.
        bool        readValue(const Deserializer &d, const char *n, bool v)               { return d.readBool(n, v); }
        int32_t     readValue(const Deserializer &d, const char *n, int32_t v)            { return d.readInt(n, v); }
        uint32_t    readValue(const Deserializer &d, const char *n, uint32_t v)           { return d.readUint(n, v); }
        float       readValue(const Deserializer &d, const char *n, float v)              { return d.readFloat(n, v); }
        std::string readValue(const Deserializer &d, const char *n, const std::string &v) { return d.readString(n, v); }
        glm::vec2   readValue(const Deserializer &d, const char *n, const glm::vec2 &v)   { return d.readVec2(n, v); }
        glm::vec3   readValue(const Deserializer &d, const char *n, const glm::vec3 &v)   { return d.readVec3(n, v); }
        glm::vec4   readValue(const Deserializer &d, const char *n, const glm::vec4 &v)   { return d.readVec4(n, v); }
        Uuid        readValue(const Deserializer &d, const char *n, const Uuid &v)        { return d.readUuid(n, v); }

        // Asset reads resolve through their own registry and have no fallback
        // overload; an absent or unresolvable id yields a null handle.
        ModelHandle    readValue(const Deserializer &d, const char *n, ModelHandle)    { return d.readModelAsset(n); }
        MaterialHandle readValue(const Deserializer &d, const char *n, MaterialHandle) { return d.readMaterialAsset(n); }

        // The same pair again with no key: one element of a sequence, where the
        // reader is already bound to the element's own node.
        void writeElementValue(Serializer &s, bool v)               { s.writeElement(v); }
        void writeElementValue(Serializer &s, int32_t v)            { s.writeElement(v); }
        void writeElementValue(Serializer &s, uint32_t v)           { s.writeElement(v); }
        void writeElementValue(Serializer &s, float v)              { s.writeElement(v); }
        void writeElementValue(Serializer &s, const std::string &v) { s.writeElement(v); }
        void writeElementValue(Serializer &s, const glm::vec2 &v)   { s.writeElement(v); }
        void writeElementValue(Serializer &s, const glm::vec3 &v)   { s.writeElement(v); }
        void writeElementValue(Serializer &s, const glm::vec4 &v)   { s.writeElement(v); }
        void writeElementValue(Serializer &s, const Uuid &v)        { s.writeElement(v); }
        void writeElementValue(Serializer &s, ModelHandle v)        { s.writeAssetElement(v); }
        void writeElementValue(Serializer &s, MaterialHandle v)     { s.writeAssetElement(v); }

        bool        readElementValue(const Deserializer &d, bool v)               { return d.readBoolValue(v); }
        int32_t     readElementValue(const Deserializer &d, int32_t v)            { return d.readIntValue(v); }
        uint32_t    readElementValue(const Deserializer &d, uint32_t v)           { return d.readUintValue(v); }
        float       readElementValue(const Deserializer &d, float v)              { return d.readFloatValue(v); }
        std::string readElementValue(const Deserializer &d, const std::string &v) { return d.readStringValue(v); }
        glm::vec2   readElementValue(const Deserializer &d, const glm::vec2 &v)   { return d.readVec2Value(v); }
        glm::vec3   readElementValue(const Deserializer &d, const glm::vec3 &v)   { return d.readVec3Value(v); }
        glm::vec4   readElementValue(const Deserializer &d, const glm::vec4 &v)   { return d.readVec4Value(v); }
        Uuid        readElementValue(const Deserializer &d, const Uuid &v)        { return d.readUuidValue(v); }
        ModelHandle    readElementValue(const Deserializer &d, ModelHandle)    { return d.readModelAssetValue(); }
        MaterialHandle readElementValue(const Deserializer &d, MaterialHandle) { return d.readMaterialAssetValue(); }

        // ------------------------------------------------------------------
        // Generic from here down.
        // ------------------------------------------------------------------
        template <class T>
        SemanticOps makeOps()
        {
            return SemanticOps{
                +[](const void *field, const char *name, Serializer &s)
                { writeValue(s, name, *static_cast<const T *>(field)); },

                +[](void *field, const char *name, Deserializer &d)
                { *static_cast<T *>(field) = readValue(d, name, *static_cast<T *>(field)); },

                +[](const void *field, Serializer &s)
                { writeElementValue(s, *static_cast<const T *>(field)); },

                +[](void *field, const Deserializer &d)
                { *static_cast<T *>(field) = readElementValue(d, *static_cast<T *>(field)); },

                +[](void *field) { *static_cast<T *>(field) = T{}; },

                +[](const void *a, const void *b)
                { return *static_cast<const T *>(a) == *static_cast<const T *>(b); },

                nullptr,   // draw -- 1.11
            };
        }

        template <class T>
        void add(SemanticRegistry &registry)
        {
            static_assert(semanticIdOf<T> != 0,
                          "type has no FAYE_SEMANTIC declaration -- see TypeDescriptor.hpp");
            registry.register_(semanticIdOf<T>, makeOps<T>());
        }

        void registerBuiltins(SemanticRegistry &registry)
        {
            add<bool>(registry);
            add<int32_t>(registry);
            add<uint32_t>(registry);
            add<float>(registry);
            add<std::string>(registry);
            add<glm::vec2>(registry);
            add<glm::vec3>(registry);
            add<glm::vec4>(registry);
            add<Uuid>(registry);
            add<ModelHandle>(registry);
            add<MaterialHandle>(registry);
        }
    }

    SemanticRegistry &SemanticRegistry::instance()
    {
        static SemanticRegistry registry = [] {
            SemanticRegistry fresh;
            registerBuiltins(fresh);
            return fresh;
        }();
        return registry;
    }

    void SemanticRegistry::register_(TypeId id, const SemanticOps &newOps)
    {
        ops[id] = newOps;
    }

    const SemanticOps *SemanticRegistry::find(TypeId id) const
    {
        const auto it = ops.find(id);
        return it == ops.end() ? nullptr : &it->second;
    }
}
