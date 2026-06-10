#include "TextureCache.hpp"

#include <stdexcept>

namespace
{
    constexpr uint64_t kFnvOffset = 14695981039346656037ull;
    constexpr uint64_t kFnvPrime = 1099511628211ull;

    size_t hashCombine(size_t seed, size_t value)
    {
        return seed ^ (value + 0x9e3779b9 + (seed << 6) + (seed >> 2));
    }
}

namespace Faye
{
    TextureCache::TextureCache(VulkanDevice &device)
        : vk_device(device)
    {
        ensureFallbackResources();
    }

    TextureCache::~TextureCache()
    {
        clear();
        for (auto &[type, resource] : fallbackTextures)
        {
            resource->destroy(vk_device.getDevice());
        }
        fallbackTextures.clear();
    }

    std::shared_ptr<VkTextureResource> TextureCache::getOrCreateTexture(const Texture &texture)
    {
        if (!texture.hasPixelData() || texture.width <= 0 || texture.height <= 0)
        {
            throw std::runtime_error("TextureCache cannot create a texture from empty data");
        }

        ensureFallbackResources();

        const TextureCacheKey key = makeKey(texture);
        const auto iterator = textureResources.find(key);
        if (iterator != textureResources.end())
        {
            return iterator->second;
        }

        auto resource = createTextureResource(texture);
        textureResources.emplace(key, resource);
        return resource;
    }

    const VkTextureResource &TextureCache::getFallbackTexture(TextureType type) const
    {
        if (const auto iterator = fallbackTextures.find(type);
            iterator != fallbackTextures.end())
        {
            return *iterator->second;
        }

        return *fallbackTextures.at(TextureType::Albedo);
    }

    void TextureCache::clear()
    {
        for (auto &[key, resource] : textureResources)
        {
            resource->destroy(vk_device.getDevice());
        }
        textureResources.clear();
    }

    void TextureCache::ensureFallbackResources()
    {
        if (!fallbackTextures.empty())
        {
            return;
        }

        fallbackTextures.emplace(TextureType::Albedo, createTextureResource({255, 255, 255, 255}, 1, 1, TextureType::Albedo));
        fallbackTextures.emplace(TextureType::Normal, createTextureResource({128, 128, 255, 255}, 1, 1, TextureType::Normal));
        fallbackTextures.emplace(TextureType::Metallic, createTextureResource({0, 0, 0, 255}, 1, 1, TextureType::Metallic));
        fallbackTextures.emplace(TextureType::Roughness, createTextureResource({255, 255, 255, 255}, 1, 1, TextureType::Roughness));
        fallbackTextures.emplace(TextureType::AmbientOcclusion, createTextureResource({255, 255, 255, 255}, 1, 1, TextureType::AmbientOcclusion));
    }

    TextureCache::TextureCacheKey TextureCache::makeKey(const Texture &texture)
    {
        return TextureCacheKey{
            texture.type,
            texture.path,
            static_cast<uint32_t>(texture.width),
            static_cast<uint32_t>(texture.height),
            static_cast<uint32_t>(texture.channels),
            texture.byteSize(),
            hashTextureData(*texture.data)};
    }

    uint64_t TextureCache::hashTextureData(const std::vector<unsigned char> &pixelData)
    {
        uint64_t hash = kFnvOffset;
        for (unsigned char byte : pixelData)
        {
            hash ^= static_cast<uint64_t>(byte);
            hash *= kFnvPrime;
        }
        return hash;
    }

    std::shared_ptr<VkTextureResource> TextureCache::createTextureResource(const Texture &texture) const
    {
        return createTextureResource(
            *texture.data,
            static_cast<uint32_t>(texture.width),
            static_cast<uint32_t>(texture.height),
            texture.type);
    }

    std::shared_ptr<VkTextureResource> TextureCache::createTextureResource(const std::vector<unsigned char> &pixelData,
                                                                           uint32_t width,
                                                                           uint32_t height,
                                                                           TextureType type) const
    {
        if (pixelData.empty() || width == 0 || height == 0)
        {
            throw std::runtime_error("TextureCache cannot upload an empty texture");
        }

        VulkanBuffer stagingBuffer(
            vk_device,
            pixelData.size(),
            1,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        stagingBuffer.map();
        stagingBuffer.writeToBuffer(const_cast<unsigned char *>(pixelData.data()));

        auto textureResource = std::make_shared<VkTextureResource>();
        textureResource->imageResource.createOwned(
            vk_device,
            VkImageResourceCreateInfo{
                {width, height, 1},
                resolveTextureFormat(type),
                VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                VK_IMAGE_ASPECT_COLOR_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_TYPE_2D,
                VK_IMAGE_VIEW_TYPE_2D,
                VK_SAMPLE_COUNT_1_BIT,
                1,
                1,
                0},
            true);

        textureResource->imageResource.transitionLayout(
            vk_device,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            VK_ACCESS_TRANSFER_WRITE_BIT);

        vk_device.copyBufferToImage(
            stagingBuffer.getBuffer(),
            textureResource->imageResource.image,
            width,
            height);

        textureResource->imageResource.transitionLayout(
            vk_device,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT);

        textureResource->samplerResource.create(vk_device);
        return textureResource;
    }

    VkFormat TextureCache::resolveTextureFormat(TextureType type)
    {
        switch (type)
        {
        case TextureType::Albedo:
            return VK_FORMAT_R8G8B8A8_SRGB;
        case TextureType::Normal:
        case TextureType::Metallic:
        case TextureType::Roughness:
        case TextureType::AmbientOcclusion:
        case TextureType::Height:
            return VK_FORMAT_R8G8B8A8_UNORM;
        }

        return VK_FORMAT_R8G8B8A8_UNORM;
    }

    size_t TextureCache::TextureCacheKeyHasher::operator()(const TextureCacheKey &key) const
    {
        size_t hash = std::hash<uint32_t>{}(static_cast<uint32_t>(key.type));
        hash = hashCombine(hash, std::hash<std::string>{}(key.path));
        hash = hashCombine(hash, std::hash<uint32_t>{}(key.width));
        hash = hashCombine(hash, std::hash<uint32_t>{}(key.height));
        hash = hashCombine(hash, std::hash<uint32_t>{}(key.channels));
        hash = hashCombine(hash, std::hash<size_t>{}(key.byteCount));
        hash = hashCombine(hash, std::hash<uint64_t>{}(key.contentHash));
        return hash;
    }
}