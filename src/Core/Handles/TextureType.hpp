#pragma once

#include <cstdint>
#include <functional>

namespace Faye
{
    // Handle to a MaterialTemplate (0 = built-in PBR).
    using MaterialTemplateHandle = uint32_t;
    static constexpr MaterialTemplateHandle kBuiltinPBRTemplateHandle = 0;

    // Texture slot within a material. Lives here (not under Renderer/) so scene
    // serialization and editor tooling can name a texture slot without pulling
    // in renderer headers. See Material.hpp for the owner.
    enum class TextureType
    {
        Albedo,
        Normal,
        Metallic,
        Roughness,
        AmbientOcclusion,
        Height,
        Equirectangular,
    };

    struct TextureTypeHasher
    {
        size_t operator()(TextureType type) const
        {
            return std::hash<uint32_t>{}(static_cast<uint32_t>(type));
        }
    };
}
