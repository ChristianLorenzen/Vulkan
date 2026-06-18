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

        if (bindlessDescriptorSet != VK_NULL_HANDLE)
        {
            assignSlot(key, *resource);
        }

        return resource;
    }

    void TextureCache::initBindless(VkDescriptorSet bindlessSet)
    {
        bindlessDescriptorSet = bindlessSet;

        // Ensure fallbacks exist and register them into the bindless set.
        ensureFallbackResources();
        for (auto &[type, resource] : fallbackTextures)
        {
            TextureCacheKey fallbackKey{type, "__fallback__", 1, 1, 4, 4, 0};
            uint32_t slot = assignSlot(fallbackKey, *resource);
            fallbackSlots[type] = slot;
        }
    }

    uint32_t TextureCache::assignSlot(const TextureCacheKey &key, const VkTextureResource &resource)
    {
        auto it = textureSlots.find(key);
        if (it != textureSlots.end())
        {
            return it->second;
        }

        uint32_t slot = nextFreeSlot++;
        textureSlots[key] = slot;

        VkDescriptorImageInfo imageInfo = resource.descriptorInfo();

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = bindlessDescriptorSet;
        write.dstBinding = 0;
        write.dstArrayElement = slot;
        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.descriptorCount = 1;
        write.pImageInfo = &imageInfo;

        vkUpdateDescriptorSets(vk_device.getDevice(), 1, &write, 0, nullptr);
        return slot;
    }

    uint32_t TextureCache::getTextureSlot(const Texture &texture) const
    {
        const TextureCacheKey key = makeKey(texture);
        const auto it = textureSlots.find(key);
        if (it != textureSlots.end())
        {
            return it->second;
        }
        // Fall back to the albedo fallback slot if texture not registered.
        return getFallbackSlot(texture.type);
    }

    uint32_t TextureCache::getFallbackSlot(TextureType type) const
    {
        const auto it = fallbackSlots.find(type);
        if (it != fallbackSlots.end())
        {
            return it->second;
        }
        // Return slot 0 as last resort (albedo fallback is always the first registered).
        return 0;
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
            VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            0,
            VK_ACCESS_2_TRANSFER_WRITE_BIT);

        vk_device.copyBufferToImage(
            stagingBuffer.getBuffer(),
            textureResource->imageResource.image,
            width,
            height);

        textureResource->imageResource.transitionLayout(
            vk_device,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
            VK_ACCESS_2_TRANSFER_WRITE_BIT,
            VK_ACCESS_2_SHADER_READ_BIT);

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