#pragma once

#include <optional>
#include <string_view>

#include "Renderer/Material/Material.hpp"

namespace Faye
{
    // String <-> enum mappings for the serializable material enums. These live
    // here (not in Material.hpp) so the scene writer/reader can round-trip
    // materials without touching the renderer header.

    std::string materialAlphaModeName(MaterialAlphaMode mode);
    std::optional<MaterialAlphaMode> materialAlphaModeFromName(std::string_view name);

    std::string materialDomainName(MaterialDomain domain);
    std::optional<MaterialDomain> materialDomainFromName(std::string_view name);

    std::string textureTypeName(TextureType type);
    std::optional<TextureType> textureTypeFromName(std::string_view name);
}
