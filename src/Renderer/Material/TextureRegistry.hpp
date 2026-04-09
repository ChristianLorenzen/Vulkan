#pragma once

#include <string>

namespace Faye
{

    struct TextureHandle
    {
        u_int32_t value = 0;

        bool isValid() const { return value != 0; }

        friend bool operator==(const TextureHandle &left, const TextureHandle &right) = default;
    };

    class TextureRegistry
    {
    public:
        static constexpr TextureHandle invalidHandle{};
        TextureHandle registerTexture(std::unique_ptr<Texture> texture);
        Texture *getTexture(TextureHandle handle);
        const Texture *getTexture(TextureHandle handle) const;

    private:
        uint32_t nextHandleValue = 1;
        std::unordered_map<uint32_t, std::unique_ptr<Texture>> textures;
    };
}