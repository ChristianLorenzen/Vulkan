#include "TextureRegistry.hpp"

using namespace Faye;

Faye::TextureHandle Faye::TextureRegistry::registerTexture(std::unique_ptr<Faye::Texture> texture)
{
    if (texture == nullptr)
    {
        throw std::runtime_error("Cannot register a null texture in TextureRegistry");
    }

    const TextureHandle handle{nextHandleValue++};
    textures.emplace(handle.value, std::move(texture));
    return handle;
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
