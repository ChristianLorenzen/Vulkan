#pragma once

#include <string>
#include <unordered_map>
#include <memory>
#include <cstdint>

#include "Material.hpp"

namespace Faye
{

    struct TextureHandle
    {
        uint32_t value = 0;

        bool isValid() const { return value != 0; }

        friend bool operator==(const TextureHandle &left, const TextureHandle &right) = default;
    };

    class TextureRegistry
    {
    public:
        static constexpr TextureHandle invalidHandle{};

        // Register a texture; always allocates a new handle.
        TextureHandle registerTexture(std::unique_ptr<Texture> texture);

        // Register a texture by file path.  If a texture with the same path and
        // type was already registered, the existing handle is returned instead of
        // creating a duplicate entry.
        TextureHandle registerOrGetTexture(const std::string &path, TextureType type);

        Texture *getTexture(TextureHandle handle);
        const Texture *getTexture(TextureHandle handle) const;

    private:
        uint32_t nextHandleValue = 1;
        std::unordered_map<uint32_t, std::unique_ptr<Texture>> textures;
        // Maps "path#type" → handle for deduplication.
        std::unordered_map<std::string, TextureHandle> pathIndex;
    };
}