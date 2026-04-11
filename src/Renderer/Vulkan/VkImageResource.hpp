#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>

#include "vk_device.hpp"

namespace Faye
{
    struct VkImageResourceCreateInfo
    {
        VkExtent3D extent{0, 0, 1};
        VkFormat format = VK_FORMAT_UNDEFINED;
        VkImageUsageFlags usage = 0;
        VkImageAspectFlags aspectFlags = 0;
        VkMemoryPropertyFlags memoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL;
        VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkImageType imageType = VK_IMAGE_TYPE_2D;
        VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_2D;
        VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
        uint32_t mipLevels = 1;
        uint32_t arrayLayers = 1;
        VkImageCreateFlags flags = 0;
    };

    struct VkImageWrapInfo
    {
        VkImage image = VK_NULL_HANDLE;
        VkExtent3D extent{0, 0, 1};
        VkFormat format = VK_FORMAT_UNDEFINED;
        VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkImageAspectFlags aspectFlags = 0;
        VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_2D;
        uint32_t mipLevels = 1;
        uint32_t arrayLayers = 1;
    };

    struct VkImageResource
    {
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkImageView imageView = VK_NULL_HANDLE;
        VkFormat format = VK_FORMAT_UNDEFINED;
        VkExtent3D extent{0, 0, 0};
        VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkImageAspectFlags aspectFlags = 0;
        VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_2D;
        uint32_t mipLevels = 0;
        uint32_t arrayLayers = 1;
        bool ownsImage = false;

        VkImageResource() = default;
        ~VkImageResource() = default;

        VkImageResource(const VkImageResource &) = delete;
        VkImageResource &operator=(const VkImageResource &) = delete;

        VkImageResource(VkImageResource &&other) noexcept;
        VkImageResource &operator=(VkImageResource &&other) noexcept;

        bool isValid() const;
        bool hasView() const;
        void reset();

        void createOwned(VulkanDevice &device,
                         const VkImageResourceCreateInfo &createInfo,
                         bool createDefaultView = true);
        void wrapExternal(VkDevice device,
                          const VkImageWrapInfo &wrapInfo,
                          bool createDefaultView = true);
        void wrapSwapchainImage(VkDevice device,
                                VkImage swapchainImage,
                                VkFormat swapchainFormat,
                                VkExtent2D swapchainExtent,
                                VkImageAspectFlags swapchainAspectFlags = VK_IMAGE_ASPECT_COLOR_BIT,
                                uint32_t swapchainMipLevels = 1,
                                VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                                bool createDefaultView = true);
        void destroy(VkDevice device);
        VkImageView createImageView(VkDevice device,
                                    uint32_t baseMipLevel = 0,
                                    uint32_t levelCount = 0,
                                    uint32_t baseArrayLayer = 0,
                                    uint32_t layerCount = 0);
        void transitionLayout(VulkanDevice &device,
                              VkImageLayout newLayout,
                              VkPipelineStageFlags srcStageMask,
                              VkPipelineStageFlags dstStageMask,
                              VkAccessFlags srcAccessMask,
                              VkAccessFlags dstAccessMask,
                              uint32_t baseMipLevel = 0,
                              uint32_t levelCount = 0,
                              uint32_t baseArrayLayer = 0,
                              uint32_t layerCount = 0);

        static VkImageAspectFlags defaultAspectFlagsForFormat(VkFormat format);
    };
}