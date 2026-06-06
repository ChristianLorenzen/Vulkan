#pragma once

#include <vulkan/vulkan.h>

#include "VkImageResource.hpp"
#include "VkSamplerResource.hpp"

namespace Faye
{
    struct VkTextureResource
    {
        VkImageResource imageResource;
        VkSamplerResource samplerResource;
        VkDescriptorSet descriptorSet = VK_NULL_HANDLE;

        VkTextureResource() = default;
        ~VkTextureResource() = default;

        VkTextureResource(const VkTextureResource &) = delete;
        VkTextureResource &operator=(const VkTextureResource &) = delete;

        VkTextureResource(VkTextureResource &&other) noexcept;
        VkTextureResource &operator=(VkTextureResource &&other) noexcept;

        bool isValid() const;
        void reset();
        void destroy(VkDevice device);
        VkDescriptorImageInfo descriptorInfo(VkImageLayout imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) const;
    };
}