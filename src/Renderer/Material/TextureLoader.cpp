#include "TextureLoader.hpp"

#include <stdexcept>

#include "Core/Logging/Logger.hpp"
#include "Core/Path/Paths.hpp"
#include "include/stb_image.h"
#include "quill/LogMacros.h"

namespace Faye
{
    Texture loadTextureFromFile(const std::string &texturePath, TextureType textureType)
    {
        int width = 0;
        int height = 0;
        int channels = 0;

        // Callers pass either form: an absolute path from a file dialog or an
        // importer, or the project-relative path a scene file stored.
        const std::string resolvedPath = Paths::fromProjectRelative(texturePath);

        unsigned char *data = stbi_load(resolvedPath.c_str(), &width, &height, &channels, STBI_rgb_alpha);
        if (data == nullptr)
        {
            LOG_ERROR(Logger::get(), "Failed to load texture at path {}. stbi error: {}", resolvedPath, stbi_failure_reason());
            throw std::runtime_error("Failed to load texture from file: " + resolvedPath);
        }

        std::vector<unsigned char> textureData(data, data + (width * height * 4));
        stbi_image_free(data);

        LOG_INFO(
            Logger::get(),
            "Loaded runtime texture at path {} with dimensions {}x{} and {} channels.",
            resolvedPath,
            width,
            height,
            channels);

        return Texture::create(std::move(textureData), width, height, channels, resolvedPath, textureType);
    }

    HdrImage loadHDRTextureFromFile(const std::string &texturePath, TextureType textureType) {
        int width = 0;
        int height = 0;
        int channels = 0;

        const std::string resolvedPath = Paths::fromProjectRelative(texturePath);

        float *data = stbi_loadf(resolvedPath.c_str(), &width, &height, &channels, STBI_rgb_alpha);
        if (data == nullptr)
        {
            LOG_ERROR(Logger::get(), "Failed to load texture at path {}. stbi error: {}", resolvedPath, stbi_failure_reason());
            throw std::runtime_error("Failed to load texture from file: " + resolvedPath);
        }

        std::vector<float> textureData(data, data + (width * height * 4));
        stbi_image_free(data);

        LOG_INFO(
            Logger::get(),
            "Loaded runtime texture at path {} with dimensions {}x{} and {} channels.",
            resolvedPath,
            width,
            height,
            channels);


        HdrImage im = {std::move(textureData), width, height};
        return im;
    }
}