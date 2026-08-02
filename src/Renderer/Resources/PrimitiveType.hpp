#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace Faye
{
    enum class PrimitiveType : uint8_t
    {
        Cube = 0,
        Sphere,
        Plane,
        Capsule,
        WaterPlane,
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
        case PrimitiveType::WaterPlane:
            return "Water Plane";
        case PrimitiveType::Count:
            break;
        }

        return "Unknown Primitive";
    }

    // Reverse lookup for scene files (name -> enum). nullopt for unknown names.
    inline std::optional<PrimitiveType> primitiveTypeFromName(std::string_view name)
    {
        for (int i = 0; i < static_cast<int>(PrimitiveType::Count); ++i)
        {
            const auto type = static_cast<PrimitiveType>(i);
            if (primitiveTypeName(type) == name)
                return type;
        }
        return std::nullopt;
    }
}