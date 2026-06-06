#pragma once

#include <cstdint>
#include <string_view>

namespace Faye
{
    enum class PrimitiveType : uint8_t
    {
        Cube = 0,
        Sphere,
        Plane,
        Capsule,
        Count,
    };

    inline constexpr std::string_view primitiveTypeName(PrimitiveType primitiveType)
    {
        switch (primitiveType)
        {
        case PrimitiveType::Cube:
            return "Cube";
        case PrimitiveType::Sphere:
            return "Sphere";
        case PrimitiveType::Plane:
            return "Plane";
        case PrimitiveType::Capsule:
            return "Capsule";
        case PrimitiveType::Count:
            break;
        }

        return "Unknown Primitive";
    }
}