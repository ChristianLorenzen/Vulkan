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
        const char *debugName = nullptr;
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
        VmaAllocation allocation = VK_NULL_HANDLE;
        VmaAllocator allocator = VK_NULL_HANDLE;
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
        void wrapExternal(VulkanDevice &device,
                          const VkImageWrapInfo &wrapInfo,
                          bool createDefaultView = true);
        void wrapSwapchainImage(VulkanDevice &device,
                                VkImage swapchainImage,
                                VkFormat swapchainFormat,
                                VkExtent2D swapchainExtent,
                                VkImageAspectFlags swapchainAspectFlags = VK_IMAGE_ASPECT_COLOR_BIT,
                                uint32_t swapchainMipLevels = 1,
                                VkImageLayout initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                                bool createDefaultView = true);
        // use for whole image transitions
        void recordTransition(VkCommandBuffer commandBuffer,
                              VkImageLayout newLayout,
                              VkPipelineStageFlags2 srcStageMask,
                              VkPipelineStageFlags2 dstStageMask,
                              VkAccessFlags2 srcAccessMask,
                              VkAccessFlags2 dstAccessMask,
                              uint32_t baseMipLevel = 0,
                              uint32_t levelCount = VK_REMAINING_MIP_LEVELS,
                              uint32_t baseArrayLayer = 0,
                              uint32_t layerCount = VK_REMAINING_ARRAY_LAYERS);
        // use for anything touching a subrange
        static void imageBarrier(VkCommandBuffer cmd, VkImage image, VkImageAspectFlags aspect,
                         VkImageLayout oldLayout, VkImageLayout newLayout,
                         VkPipelineStageFlags2 srcStage, VkPipelineStageFlags2 dstStage,
                         VkAccessFlags2 srcAccess, VkAccessFlags2 dstAccess,
                         uint32_t baseMipLevel = 0,
                         uint32_t levelCount = VK_REMAINING_MIP_LEVELS,
                         uint32_t baseArrayLayer = 0,
                         uint32_t layerCount = VK_REMAINING_ARRAY_LAYERS);
        void destroy(VkDevice device);
        VkImageView createImageView(VkDevice device,
                                    uint32_t baseMipLevel = 0,
                                    uint32_t levelCount = 0,
                                    uint32_t baseArrayLayer = 0,
                                    uint32_t layerCount = 0);
        // Creates a standalone view the caller owns and must destroy. unlike
        // createImageView this does NOT touch `imageView`, so it is safe to call
        // repeatedly
        VkImageView makeView(VkDevice device,
                             VkImageViewType type,
                             uint32_t baseMipLevel = 0,
                             uint32_t levelCount = VK_REMAINING_MIP_LEVELS,
                             uint32_t baseArrayLayer = 0,
                             uint32_t layerCount = VK_REMAINING_ARRAY_LAYERS) const;
        void transitionLayout(VulkanDevice &device,
                              VkImageLayout newLayout,
                              VkPipelineStageFlags2 srcStageMask,
                              VkPipelineStageFlags2 dstStageMask,
                              VkAccessFlags2 srcAccessMask,
                              VkAccessFlags2 dstAccessMask,
                              uint32_t baseMipLevel = 0,
                              uint32_t levelCount = 0,
                              uint32_t baseArrayLayer = 0,
                              uint32_t layerCount = 0);

        static VkImageAspectFlags defaultAspectFlagsForFormat(VkFormat format);
    };
}