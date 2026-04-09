#pragma once

#include <vulkan/vulkan.h>

#include "vk_device.hpp"

namespace Faye
{
    struct VkSamplerResourceCreateInfo
    {
        VkFilter magFilter = VK_FILTER_LINEAR;
        VkFilter minFilter = VK_FILTER_LINEAR;
        VkSamplerMipmapMode mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        VkSamplerAddressMode addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        VkSamplerAddressMode addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        VkSamplerAddressMode addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        bool enableAnisotropy = true;
        float maxAnisotropy = 0.0f;
        float mipLodBias = 0.0f;
        float minLod = 0.0f;
        float maxLod = 0.0f;
        VkBorderColor borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        VkBool32 unnormalizedCoordinates = VK_FALSE;
        VkBool32 compareEnable = VK_FALSE;
        VkCompareOp compareOp = VK_COMPARE_OP_ALWAYS;
    };

    struct VkSamplerResource
    {
        VkSampler sampler = VK_NULL_HANDLE;

        VkSamplerResource() = default;
        ~VkSamplerResource() = default;

        VkSamplerResource(const VkSamplerResource &) = delete;
        VkSamplerResource &operator=(const VkSamplerResource &) = delete;

        VkSamplerResource(VkSamplerResource &&other) noexcept;
        VkSamplerResource &operator=(VkSamplerResource &&other) noexcept;

        bool isValid() const;
        void reset();
        void create(VulkanDevice &device, const VkSamplerResourceCreateInfo &createInfo = {});
        void destroy(VkDevice device);
        VkDescriptorImageInfo descriptorInfo(VkImageView imageView,
                                             VkImageLayout imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) const;
    };
}