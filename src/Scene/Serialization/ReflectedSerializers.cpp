#include "Scene/Serialization/ReflectedSerializers.hpp"

#include "Core/ECS/Reflection/SemanticRegistry.hpp"
#include "Scene/Serialization/Deserializer.hpp"
#include "Scene/Serialization/Serializer.hpp"

namespace Faye::Ecs
{
    namespace
    {
        const SemanticOps *semanticFor(const FieldDescriptor &f)
        {
            return f.typeId == 0 ? nullptr : SemanticRegistry::instance().find(f.typeId);
        }

        const void *at(const void *base, const FieldDescriptor &f)
        {
            return static_cast<const char *>(base) + f.offset;
        }

        void *at(void *base, const FieldDescriptor &f)
        {
            return static_cast<char *>(base) + f.offset;
        }

        // FieldOps::elementAt is the only accessor a container exposes and it
        // takes void*. Writing needs the same traversal from a const object; we
        // read through the result and never write, so the cast is safe.
        void *elementOf(const FieldOps &ops, const void *container, size_t index)
        {
            return ops.elementAt(const_cast<void *>(container), index);
        }

        void writeElement(const FieldDescriptor &element, const void *value, Serializer &s);
        void readElement(const FieldDescriptor &element, void *value, const Deserializer &d);

        void writeMembers(const TypeDescriptor &type, const void *object, Serializer &s);
        void readMembers(const TypeDescriptor &type, void *object, Deserializer &d);

        // ---- write --------------------------------------------------------

        void writeSequence(const FieldDescriptor &f, const void *container, Serializer &s)
        {
            const size_t count = f.ops->size(container);
            for (size_t i = 0; i < count; ++i)
                writeElement(*f.element, elementOf(*f.ops, container, i), s);
        }

        void writeElement(const FieldDescriptor &element, const void *value, Serializer &s)
        {
            // vector<SomeEnum>: same by-name rule as a plain enum field.
            if (element.enumOps != nullptr)
            {
                const int64_t stored = element.enumOps->read(value);
                if (const char *enumerator = element.enumOps->nameOf(stored))
                    s.writeElement(std::string{enumerator});
                else
                    s.writeElement(static_cast<int32_t>(stored));
                return;
            }
            if (const SemanticOps *ops = semanticFor(element))
            {
                ops->serializeElement(value, s);
                return;
            }
            if (element.layout == Layout::Struct)
            {
                s.beginMap();
                writeMembers(*element.nested, value, s);
                s.endMap();
                return;
            }
            // DynArray: isSerializableField vetted this before we got here.
            s.beginSequence();
            writeSequence(element, value, s);
            s.endSequence();
        }

        // Enums persist by NAME, matching the hand-written material enum tables
        // this generalises: a save then survives someone inserting an
        // enumerator in the middle, which an ordinal would silently
        // reinterpret. A value matching no enumerator falls back to the number
        // rather than being dropped -- that is a save from a newer build, or an
        // int cast in, and losing it would be worse than writing it opaquely.
        void writeEnum(const EnumOps &ops, const void *value, const char *name, Serializer &s)
        {
            const int64_t stored = ops.read(value);
            if (const char *enumerator = ops.nameOf(stored))
                s.writeField(name, std::string{enumerator});
            else
                s.writeField(name, static_cast<int32_t>(stored));
        }

        // YAML scalars are untyped, so `enumVal: 2` reads back as the STRING
        // "2" -- there is no node-level string/int distinction to branch on.
        // The order below is what disambiguates: name first, then digits.
        bool looksNumeric(const std::string &text)
        {
            return !text.empty() && (text[0] == '-' || (text[0] >= '0' && text[0] <= '9'));
        }

        void readEnum(const EnumOps &ops, void *value, const char *key, const Deserializer &d)
        {
            const std::string text = d.readString(key, std::string{});
            if (text.empty())
                return;   // absent -> keep the constructed default

            int64_t resolved = 0;
            if (ops.valueOf(text.c_str(), resolved))
            {
                ops.write(value, resolved);
                return;
            }

            // Not an enumerator this build knows. A number is what the writer
            // emits for a value with no name, so take it verbatim rather than
            // losing it; anything else is an enumerator removed since the save
            // was written, and the default is the safer answer.
            if (looksNumeric(text))
                ops.write(value, d.readInt(key, static_cast<int32_t>(ops.read(value))));
        }

        void writeField(const FieldDescriptor &f, const void *object, Serializer &s)
        {
            if (hasFlag(f.flags, FieldFlags::NotSerialized) || !isSerializableField(f))
                return;

            const void *value = at(object, f);

            if (f.enumOps != nullptr)
            {
                writeEnum(*f.enumOps, value, f.name, s);
                return;
            }
            if (const SemanticOps *ops = semanticFor(f))
            {
                ops->serialize(value, f.name, s);
                return;
            }
            if (f.layout == Layout::Struct)
            {
                s.beginFieldMap(f.name);
                writeMembers(*f.nested, value, s);
                s.endMap();
                return;
            }
            s.beginFieldSequence(f.name);
            writeSequence(f, value, s);
            s.endSequence();
        }

        void writeMembers(const TypeDescriptor &type, const void *object, Serializer &s)
        {
            for (uint32_t i = 0; i < type.fieldCount; ++i)
                writeField(type.fields[i], object, s);
        }

        // ---- read ---------------------------------------------------------

        // Primary key, or the migration alias when the primary is absent and
        // the alias is present. Never the alias when both exist.
        const char *keyOf(const FieldDescriptor &f, const Deserializer &d)
        {
            if (f.serializedAs != nullptr && !d.has(f.name) && d.has(f.serializedAs))
                return f.serializedAs;
            return f.name;
        }

        void readSequence(const FieldDescriptor &f, void *container, const Deserializer &sequence)
        {
            const size_t count = sequence.sizeValue();
            f.ops->clear(container);
            f.ops->resize(container, count);
            for (size_t i = 0; i < count; ++i)
                readElement(*f.element, f.ops->elementAt(container, i), sequence.atValue(i));
        }

        void readElement(const FieldDescriptor &element, void *value, const Deserializer &d)
        {
            if (element.enumOps != nullptr)
            {
                const EnumOps &ops = *element.enumOps;
                const std::string text = d.readStringValue(std::string{});
                int64_t resolved = 0;
                if (ops.valueOf(text.c_str(), resolved))
                    ops.write(value, resolved);
                else if (looksNumeric(text))
                    ops.write(value, d.readIntValue(static_cast<int32_t>(ops.read(value))));
                return;
            }
            if (const SemanticOps *ops = semanticFor(element))
            {
                ops->deserializeElement(value, d);
                return;
            }
            if (element.layout == Layout::Struct)
            {
                if (!d.isMapValue())
                    return;
                Deserializer nested = d;
                readMembers(*element.nested, value, nested);
                return;
            }
            if (d.isSequenceValue())
                readSequence(element, value, d);
        }

        void readField(const FieldDescriptor &f, void *object, Deserializer &d)
        {
            if (hasFlag(f.flags, FieldFlags::NotSerialized) || !isSerializableField(f))
                return;

            const char *key = keyOf(f, d);
            void *value = at(object, f);

            if (f.enumOps != nullptr)
            {
                readEnum(*f.enumOps, value, key, d);
                return;
            }
            if (const SemanticOps *ops = semanticFor(f))
            {
                // The semantic ops take the CURRENT value as fallback, so an
                // absent key leaves the default in place. Do not pre-check
                // has(): that is what makes optional fields round-trip
                // byte-identically.
                ops->deserialize(value, key, d);
                return;
            }

            const Deserializer child = d.child(key);
            if (f.layout == Layout::Struct)
            {
                if (!child.isMapValue())
                    return;   // absent -> keep constructed defaults
                Deserializer nested = child;
                readMembers(*f.nested, value, nested);
                return;
            }
            if (child.isSequenceValue())
                readSequence(f, value, child);
            // An absent sequence leaves the default contents alone rather than
            // clearing them -- same rule as every other field.
        }

        void readMembers(const TypeDescriptor &type, void *object, Deserializer &d)
        {
            for (uint32_t i = 0; i < type.fieldCount; ++i)
                readField(type.fields[i], object, d);
        }
    }

    bool isSerializableField(const FieldDescriptor &field)
    {
        // Checked before the semantic: an enum's layout is its underlying
        // integer, which may or may not have a semantic of its own, and the
        // name table is the better answer either way.
        if (field.enumOps != nullptr)
            return field.enumOps->entries != nullptr && field.enumOps->read != nullptr &&
                   field.enumOps->write != nullptr;
        if (field.typeId != 0)
            return SemanticRegistry::instance().find(field.typeId) != nullptr;
        if (field.layout == Layout::Struct)
            return field.nested != nullptr;
        if (field.layout == Layout::DynArray)
            return field.ops != nullptr && field.element != nullptr &&
                   isSerializableField(*field.element);
        return false;
    }

    void serializeReflected(const TypeDescriptor &type, const void *component, Serializer &s)
    {
        writeMembers(type, component, s);
    }

    void deserializeReflected(const TypeDescriptor &type, void *component, Deserializer &d)
    {
        readMembers(type, component, d);
    }
}
