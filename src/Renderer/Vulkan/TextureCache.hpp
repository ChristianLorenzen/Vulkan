#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <unordered_map>

#include "Renderer/Material/Material.hpp"
#include "VkTextureResource.hpp"
#include "VulkanBuffer.hpp"

namespace Faye
{
    class TextureCache
    {
    public:
        explicit TextureCache(VulkanDevice &device);
        ~TextureCache();

        TextureCache(const TextureCache &) = delete;
        TextureCache &operator=(const TextureCache &) = delete;
        TextureCache(TextureCache &&) = delete;
        TextureCache &operator=(TextureCache &&) = delete;

        std::shared_ptr<VkTextureResource> getOrCreateTexture(const Texture &texture);
        const VkTextureResource &getFallbackTexture(TextureType type) const;
        void clear();

        // Bindless texture registry. Must be called once after the bindless descriptor set
        // is allocated. All subsequent getOrCreateTexture calls will register new textures
        // into the bindless array automatically.
        void initBindless(VkDescriptorSet bindlessSet);

        // Returns the bindless slot index for the given texture.
        // getOrCreateTexture must have been called for this texture first.
        uint32_t getTextureSlot(const Texture &texture) const;
        uint32_t getFallbackSlot(TextureType type) const;

    private:
        struct TextureCacheKey
        {
            TextureType type = TextureType::Albedo;
            std::string path;
            uint32_t width = 0;
            uint32_t height = 0;
            uint32_t channels = 0;
            size_t byteCount = 0;
            uint64_t contentHash = 0;

            friend bool operator==(const TextureCacheKey &left, const TextureCacheKey &right) = default;
        };

        struct TextureCacheKeyHasher
        {
            size_t operator()(const TextureCacheKey &key) const;
        };

        VulkanDevice &vk_device;
        std::unordered_map<TextureCacheKey, std::shared_ptr<VkTextureResource>, TextureCacheKeyHasher> textureResources;
        std::unordered_map<TextureType, std::shared_ptr<VkTextureResource>, TextureTypeHasher> fallbackTextures;

        // Bindless slot tracking
        VkDescriptorSet bindlessDescriptorSet{VK_NULL_HANDLE};
        uint32_t nextFreeSlot{0};
        std::unordered_map<TextureCacheKey, uint32_t, TextureCacheKeyHasher> textureSlots;
        std::unordered_map<TextureType, uint32_t, TextureTypeHasher> fallbackSlots;

        uint32_t assignSlot(const TextureCacheKey &key, const VkTextureResource &resource);

        void ensureFallbackResources();
        static TextureCacheKey makeKey(const Texture &texture);
        static uint64_t hashTextureData(const std::vector<unsigned char> &pixelData);
        std::shared_ptr<VkTextureResource> createTextureResource(const Texture &texture) const;
        std::shared_ptr<VkTextureResource> createTextureResource(const std::vector<unsigned char> &pixelData,
                                                                 uint32_t width,
                                                                 uint32_t height,
                                                                 TextureType type) const;
        static VkFormat resolveTextureFormat(TextureType type);
    };
}