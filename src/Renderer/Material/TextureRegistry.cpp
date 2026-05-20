#include "TextureRegistry.hpp"
#include "TextureLoader.hpp"

#include <stdexcept>

using namespace Faye;

Faye::TextureHandle Faye::TextureRegistry::registerTexture(std::unique_ptr<Faye::Texture> texture)
{
    if (texture == nullptr)
    {
        throw std::runtime_error("Cannot register a null texture in TextureRegistry");
    }

    const TextureHandle handle{nextHandleValue++};

    if (!texture->path.empty())
    {
        const std::string key = texture->path + "#" + std::to_string(static_cast<uint32_t>(texture->type));
        pathIndex.emplace(key, handle);
    }

    textures.emplace(handle.value, std::move(texture));
    return handle;
}

Faye::TextureHandle Faye::TextureRegistry::registerOrGetTexture(const std::string &path, TextureType type)
{
    const std::string key = path + "#" + std::to_string(static_cast<uint32_t>(type));
    if (const auto iterator = pathIndex.find(key); iterator != pathIndex.end())
    {
        return iterator->second;
    }

    Texture loaded = loadTextureFromFile(path, type);
    return registerTexture(std::make_unique<Texture>(std::move(loaded)));
}

Faye::Texture *Faye::TextureRegistry::getTexture(TextureHandle handle)
{
    auto iterator = textures.find(handle.value);
    return iterator != textures.end() ? iterator->second.get() : nullptr;
}

const Faye::Texture *Faye::TextureRegistry::getTexture(TextureHandle handle) const
{
    auto iterator = textures.find(handle.value);
    return iterator != textures.end() ? iterator->second.get() : nullptr;
}
