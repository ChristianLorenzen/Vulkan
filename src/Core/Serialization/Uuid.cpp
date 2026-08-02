#include "Core/Serialization/Uuid.hpp"

#include <boost/uuid/name_generator.hpp>
#include <boost/uuid/random_generator.hpp>
#include <boost/uuid/string_generator.hpp>
#include <boost/uuid/uuid_io.hpp>

namespace Faye
{
    namespace
    {
        // Well-known Faye namespace UUID for name-based (v5-style) ids.
        // Changing it changes every deterministic asset id — keep it stable.
        const boost::uuids::uuid kFayeNamespaceUuid = {{
            0xf3, 0x0a, 0x1b, 0x2c, 0x3d, 0x4e, 0x5f, 0x60,
            0x71, 0x82, 0x93, 0xa4, 0xb5, 0xc6, 0xd7, 0xe8,
        }};
    }

    Uuid Uuid::generateV4()
    {
        // thread_local so concurrent generators don't share mt19937 state;
        // static so we don't re-seed from std::random_device on every call.
        static thread_local boost::uuids::random_generator_mt19937 generator;
        return Uuid{generator()};
    }

    Uuid Uuid::nameBased(const std::string &name)
    {
        boost::uuids::name_generator generator(kFayeNamespaceUuid);
        return Uuid{generator("faye:" + name)};
    }

    Uuid Uuid::fromString(const std::string &text)
    {
        boost::uuids::string_generator generator;
        return Uuid{generator(text)};
    }

    std::string Uuid::toString() const
    {
        return boost::uuids::to_string(value);
    }

    bool Uuid::isNull() const
    {
        return value.is_nil();
    }

    bool Uuid::operator==(const Uuid &other) const
    {
        return value == other.value;
    }

    bool Uuid::operator<(const Uuid &other) const
    {
        return value < other.value;
    }
}

std::size_t std::hash<Faye::Uuid>::operator()(const Faye::Uuid &uuid) const noexcept
{
    return boost::uuids::hash_value(uuid.value);
}
