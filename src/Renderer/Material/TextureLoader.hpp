#pragma once

#include <string>

#include "Material.hpp"

namespace Faye
{
    Texture loadTextureFromFile(const std::string &texturePath, TextureType textureType);

    HdrImage loadHDRTextureFromFile(const std::string &texturePath, TextureType textureType);
}