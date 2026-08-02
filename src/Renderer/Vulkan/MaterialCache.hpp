#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <unordered_map>

#include <glm/glm.hpp>

#include "Renderer/Material/Material.hpp"
#include "Renderer/Material/MaterialRegistry.hpp"
#include "TextureCache.hpp"
#include "VkTextureResource.hpp"
#include "VulkanBuffer.hpp"
#include "vk_descriptors.hpp"

namespace Faye
{
    struct MaterialUniformData
    {
        alignas(16) glm::vec4 baseColorFactor{1.0f, 1.0f, 1.0f, 1.0f};
        alignas(16) glm::vec4 surfaceFactors{0.0f, 1.0f, 1.0f, 1.0f};
        alignas(16) glm::vec4 specularShininess{1.0f, 1.0f, 1.0f, 32.0f};
        alignas(16) glm::vec4 emissiveFactors{0.0f, 0.0f, 0.0f, 1.0f};
        alignas(16) glm::vec4 alphaModeCutoff{0.0f, 0.5f, 0.0f, 0.0f};
    };

    struct MaterialState
    {
        MaterialHandle handle{};
        uint64_t materialRevision = 0;
        MaterialPipelineConfig pipelineConfig{};
        VkDescriptorSet descriptorSet = VK_NULL_HANDLE; // params UBO only (set 1, binding 0)
        std::unique_ptr<VulkanBuffer> parameterBuffer{};
        MaterialUniformData uniformData{};
        // Bindless texture slot indices (written into push constants per draw call).
        uint32_t albedoSlot{0};
        uint32_t normalSlot{0};
        uint32_t metallicSlot{0};
        uint32_t roughnessSlot{0};
        uint32_t aoSlot{0};

        void reset();
    };

    class MaterialCache
    {
    public:
        MaterialCache(VulkanDevice &device,
                      TextureCache &textureCache,
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
        TextureCache &textureCache;
        VulkanDescriptorSetLayout &materialSetLayout;
        VulkanDescriptorPool &materialPool;
        std::unordered_map<uint32_t, MaterialState> materialStates;

        void refreshState(MaterialState &state, MaterialHandle handle, const Material &material);
        void writeParamDescriptorSet(MaterialState &state);
        void destroyState(MaterialState &state);
    };
}