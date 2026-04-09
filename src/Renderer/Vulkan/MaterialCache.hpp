#pragma once

#include <cstddef>
#include <cstdint>
#include <unordered_map>

#include "Renderer/Material/Material.hpp"
#include "Renderer/Material/MaterialRegistry.hpp"
#include "VkTextureResource.hpp"
#include "VulkanBuffer.hpp"
#include "vk_descriptors.hpp"

namespace Faye
{
    struct MaterialState
    {
        struct TextureTypeHasher
        {
            size_t operator()(TextureType type) const
            {
                return std::hash<uint32_t>{}(static_cast<uint32_t>(type));
            }
        };

        MaterialHandle handle{};
        MaterialPipelineConfig pipelineConfig{};
        VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
        std::unordered_map<TextureType, VkTextureResource, TextureTypeHasher> textures;

        void reset();
        const VkTextureResource *findTexture(TextureType type) const;
    };

    class MaterialCache
    {
    public:
        MaterialCache(VulkanDevice &device,
                      VulkanDescriptorSetLayout &materialSetLayout,
                      VulkanDescriptorPool &materialPool);
        ~MaterialCache();

        MaterialCache(const MaterialCache &) = delete;
        MaterialCache &operator=(const MaterialCache &) = delete;
        MaterialCache(MaterialCache &&) = delete;
        MaterialCache &operator=(MaterialCache &&) = delete;

        const MaterialState &getOrCreateState(MaterialHandle handle, const Material &material);
        const MaterialState *findState(MaterialHandle handle) const;

        void invalidateMaterial(MaterialHandle handle);
        void clear();

    private:
        VulkanDevice &vk_device;
        VulkanDescriptorSetLayout &materialSetLayout;
        VulkanDescriptorPool &materialPool;
        std::unordered_map<uint32_t, MaterialState> materialStates;
        VkTextureResource fallbackAlbedoTexture;

        void ensureFallbackResources();
        void buildState(MaterialState &state, MaterialHandle handle, const Material &material);
        void writeDescriptorSet(MaterialState &state);
        void destroyState(MaterialState &state);

        VkTextureResource createTextureResource(const Texture &texture) const;
        VkTextureResource createTextureResource(const std::vector<unsigned char> &pixelData,
                                               uint32_t width,
                                               uint32_t height,
                                               TextureType type) const;
        const VkTextureResource &resolveBoundTexture(const MaterialState &state) const;
        static const Texture *selectTexture(const MaterialData &materialData, TextureType type);
        static VkFormat resolveTextureFormat(TextureType type);
    };
}