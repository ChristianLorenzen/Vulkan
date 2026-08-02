#pragma once

namespace Faye::Ecs
{
    class Serializer;
    class Deserializer;

    // Hand-written serialize/deserialize thunks for the registered engine
    // components. Signatures match the reserved ComponentTypeInfo slots; each
    // pair is passed to ComponentTypeRegistry::registerType<T>(...).
    //
    // Field names are the C++ data member names (reflection-aligned).
    void serializeTransform(const void *, Serializer &);
    void deserializeTransform(void *, Deserializer &);
    void serializeMesh(const void *, Serializer &);
    void deserializeMesh(void *, Deserializer &);
    void serializeCamera(const void *, Serializer &);
    void deserializeCamera(void *, Deserializer &);
    void serializeWater(const void *, Serializer &);
    void deserializeWater(void *, Deserializer &);
    void serializePointLight(const void *, Serializer &);
    void deserializePointLight(void *, Deserializer &);
    void serializeDirectionalLight(const void *, Serializer &);
    void deserializeDirectionalLight(void *, Deserializer &);
    void serializePostProcessStack(const void *, Serializer &);
    void deserializePostProcessStack(void *, Deserializer &);
    void serializeRigidBody2d(const void *, Serializer &);
    void deserializeRigidBody2d(void *, Deserializer &);
    void serializeLuaScript(const void *, Serializer &);
    void deserializeLuaScript(void *, Deserializer &);
    void serializeNativeScript(const void *, Serializer &);
    void deserializeNativeScript(void *, Deserializer &);
}
