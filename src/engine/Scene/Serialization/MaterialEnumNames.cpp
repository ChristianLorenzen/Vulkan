#include "engine/Scene/Serialization/MaterialEnumNames.hpp"
namespace Faye
{
    std::string materialAlphaModeName(MaterialAlphaMode mode)
    {
        switch (mode)
        {
        case MaterialAlphaMode::Opaque:
            return "opaque";
        case MaterialAlphaMode::Mask:
            return "mask";
        }
        return "opaque";
    }

    std::optional<MaterialAlphaMode> materialAlphaModeFromName(std::string_view name)
    {
        if (name == "opaque")
            return MaterialAlphaMode::Opaque;
        if (name == "mask")
            return MaterialAlphaMode::Mask;
        return std::nullopt;
    }

    std::string materialDomainName(MaterialDomain domain)
    {
        switch (domain)
        {
        case MaterialDomain::Opaque:
            return "opaque";
        case MaterialDomain::Transparent:
            return "transparent";
        case MaterialDomain::Water:
            return "water";
        }
        return "opaque";
    }

    std::optional<MaterialDomain> materialDomainFromName(std::string_view name)
    {
        if (name == "opaque")
            return MaterialDomain::Opaque;
        if (name == "transparent")
            return MaterialDomain::Transparent;
        if (name == "water")
            return MaterialDomain::Water;
        return std::nullopt;
    }

    std::string textureTypeName(TextureType type)
    {
        switch (type)
        {
        case TextureType::Albedo:
            return "albedo";
        case TextureType::Normal:
            return "normal";
        case TextureType::Metallic:
            return "metallic";
        case TextureType::Roughness:
            return "roughness";
        case TextureType::AmbientOcclusion:
            return "ambientOcclusion";
        case TextureType::Height:
            return "height";
        case TextureType::Equirectangular:
            return "equirectangular";
        }
        return "albedo";
    }

    std::optional<TextureType> textureTypeFromName(std::string_view name)
    {
        if (name == "albedo")
            return TextureType::Albedo;
        if (name == "normal")
            return TextureType::Normal;
        if (name == "metallic")
            return TextureType::Metallic;
        if (name == "roughness")
            return TextureType::Roughness;
        if (name == "ambientOcclusion")
            return TextureType::AmbientOcclusion;
        if (name == "height")
            return TextureType::Height;
        if (name == "equirectangular")
            return TextureType::Equirectangular;
        return std::nullopt;
    }
}
