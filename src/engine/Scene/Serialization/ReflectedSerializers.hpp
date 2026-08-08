#pragma once

#include "Core/ECS/Reflection/TypeDescriptor.hpp"

namespace Faye::Ecs
{
    class Serializer;
    class Deserializer;

    // Generic (de)serialisation driven by a runtime TypeDescriptor. No
    // reflection and no <meta> -- these work on descriptor data alone, which is
    // what lets a plugin-supplied descriptor go through the same path.
    //
    // Dispatch order, applied identically to fields and to container elements:
    //
    //   1. FieldFlags::NotSerialized      -> skip
    //   2. typeId hits SemanticRegistry   -> the registered ops own it
    //   3. Layout::Struct                 -> recurse through `nested` as a map
    //   4. Layout::DynArray               -> walk `ops`, recurse on `element`
    //   5. otherwise                      -> skip (Opaque with no semantic)
    //
    // Annotations never affect bytes: no clamping to Range on read, ever. Widen
    // a range later and old scenes would read back differently than written.
    void serializeReflected(const TypeDescriptor &type, const void *component, Serializer &s);
    void deserializeReflected(const TypeDescriptor &type, void *component, Deserializer &d);

    // True when the walker can actually round-trip this field: every leaf it
    // reaches is either a registered semantic or a descendable structure.
    // serializeReflected skips fields that fail this rather than emitting a
    // partial sequence, so a field is either written whole or not at all.
    bool isSerializableField(const FieldDescriptor &field);
}

// DELIBERATELY NO <meta> HERE, and no per-T `serializeReflectedFor<T>` binder.
//
// A binder template would force this header to include Describe.hpp, and this
// header is reached from the scene writer and reader. Registration instead
// hands ComponentTypeInfo a `const TypeDescriptor *` (plain data), so the only
// files that ever see <meta> are the ones that literally name kDescriptor<T>:
// RegisterComponents.cpp and the reflection tests. Ground rule 1 holds.
