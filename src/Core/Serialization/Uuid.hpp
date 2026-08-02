#pragma once

#include <boost/uuid/uuid.hpp>

#include <functional>
#include <string>

namespace Faye
{
    // Thin wrapper over boost::uuids::uuid so the rest of the engine never
    // includes Boost directly (Boost stays behind this header).
    //
    // - generateV4():   random RFC 4122 v4 (used for entity GUIDs, scene
    //                   UUIDs, runtime-created asset IDs).
    // - nameBased():    deterministic SHA-1 "v5-style" id over "faye:" + name
    //                   (used for file-sourced asset IDs — stable across runs
    //                   and machines, no sidecar DB required).
    // - fromString()/toString(): canonical lowercase 8-4-4-4-12 text form.
    class Uuid
    {
    public:
        Uuid() = default;                        // null UUID (00000000-...)

        static Uuid generateV4();
        static Uuid nameBased(const std::string &name);
        // Throws std::runtime_error if `text` is not a valid UUID string
        // (boost::uuids::string_generator's documented error type).
        static Uuid fromString(const std::string &text);

        std::string toString() const;
        bool isNull() const;

        bool operator==(const Uuid &) const;
        bool operator<(const Uuid &) const;

    private:
        friend struct std::hash<Faye::Uuid>;
        explicit Uuid(boost::uuids::uuid value) : value(value) {}
        boost::uuids::uuid value{};
    };
}

template <>
struct std::hash<Faye::Uuid>
{
    std::size_t operator()(const Faye::Uuid &uuid) const noexcept;
};
