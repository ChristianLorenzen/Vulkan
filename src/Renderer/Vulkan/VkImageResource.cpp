#include "VkImageResource.hpp"

#include <stdexcept>
#include <utility>

namespace
{
    bool isDepthFormat(VkFormat format)
    {
        return format == VK_FORMAT_D16_UNORM ||
               format == VK_FORMAT_X8_D24_UNORM_PACK32 ||
               format == VK_FORMAT_D32_SFLOAT ||
               format == VK_FORMAT_D16_UNORM_S8_UINT ||
               format == VK_FORMAT_D24_UNORM_S8_UINT ||
               format == VK_FORMAT_D32_SFLOAT_S8_UINT;
    }

    bool hasStencilComponent(VkFormat format)
    {
        return format == VK_FORMAT_D16_UNORM_S8_UINT ||
               format == VK_FORMAT_D24_UNORM_S8_UINT ||
               format == VK_FORMAT_D32_SFLOAT_S8_UINT;
    }
}

Faye::VkImageResource::VkImageResource(VkImageResource &&other) noexcept
{
    *this = std::move(other);
}

Faye::VkImageResource &Faye::VkImageResource::operator=(VkImageResource &&other) noexcept
{
    if (this == &other)
    {
        return *this;
    }

    image = std::exchange(other.image, VK_NULL_HANDLE);
    allocation = std::exchange(other.allocation, VK_NULL_HANDLE);
    allocator = std::exchange(other.allocator, VK_NULL_HANDLE);
    imageView = std::exchange(other.imageView, VK_NULL_HANDLE);
    format = std::exchange(other.format, VK_FORMAT_UNDEFINED);
    extent = std::exchange(other.extent, VkExtent3D{0, 0, 0});
    layout = std::exchange(other.layout, VK_IMAGE_LAYOUT_UNDEFINED);
    aspectFlags = std::exchange(other.aspectFlags, 0);
    viewType = std::exchange(other.viewType, VK_IMAGE_VIEW_TYPE_2D);
    mipLevels = std::exchange(other.mipLevels, 0u);
    arrayLayers = std::exchange(other.arrayLayers, 1u);
    ownsImage = std::exchange(other.ownsImage, false);
    return *this;
}

bool Faye::VkImageResource::isValid() const
{
    return image != VK_NULL_HANDLE;
}

bool Faye::VkImageResource::hasView() const
{
    return imageView != VK_NULL_HANDLE;
}

void Faye::VkImageResource::reset()
{
    image = VK_NULL_HANDLE;
    allocation = VK_NULL_HANDLE;
    allocator = VK_NULL_HANDLE;
    imageView = VK_NULL_HANDLE;
    format = VK_FORMAT_UNDEFINED;
    extent = {0, 0, 0};
    layout = VK_IMAGE_LAYOUT_UNDEFINED;
    aspectFlags = 0;
    viewType = VK_IMAGE_VIEW_TYPE_2D;
    mipLevels = 0;
    arrayLayers = 1;
    ownsImage = false;
}

void Faye::VkImageResource::createOwned(VulkanDevice &device,
                                        const VkImageResourceCreateInfo &createInfo,
                                        bool createDefaultView)
{
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = createInfo.imageType;
    imageInfo.extent = createInfo.extent;
    imageInfo.mipLevels = createInfo.mipLevels;
    imageInfo.arrayLayers = createInfo.arrayLayers;
    imageInfo.format = createInfo.format;
    imageInfo.tiling = createInfo.tiling;
    imageInfo.initialLayout = createInfo.initialLayout;
    imageInfo.usage = createInfo.usage;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.samples = createInfo.samples;
    imageInfo.flags = createInfo.flags;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;

    device.createImageWithInfo(imageInfo, allocInfo, image, allocation);
    allocator = device.getAllocator();
    ownsImage = true;

    format = createInfo.format;
    extent = createInfo.extent;
    layout = createInfo.initialLayout;
    aspectFlags = createInfo.aspectFlags != 0
                      ? createInfo.aspectFlags
                      : defaultAspectFlagsForFormat(createInfo.format);
    viewType = createInfo.viewType;
    mipLevels = createInfo.mipLevels;
    arrayLayers = createInfo.arrayLayers;
    ownsImage = true;

    if (createDefaultView)
        createImageView(device.getDevice());
}

void Faye::VkImageResource::wrapExternal(VulkanDevice &device,
                                         const VkImageWrapInfo &wrapInfo,
                                         bool createDefaultView)
{
    destroy(device.getDevice());

    if (wrapInfo.image == VK_NULL_HANDLE)
    {
        throw std::runtime_error("Cannot wrap a null VkImage");
    }
    if (wrapInfo.format == VK_FORMAT_UNDEFINED)
    {
        throw std::runtime_error("Wrapped VkImageResource requires a valid VkFormat");
    }

    image = wrapInfo.image;
    format = wrapInfo.format;
    extent = wrapInfo.extent;
    layout = wrapInfo.layout;
    aspectFlags = wrapInfo.aspectFlags != 0 ? wrapInfo.aspectFlags : defaultAspectFlagsForFormat(wrapInfo.format);
    viewType = wrapInfo.viewType;
    mipLevels = wrapInfo.mipLevels;
    arrayLayers = wrapInfo.arrayLayers;
    ownsImage = false;

    if (createDefaultView)
    {
        createImageView(device.getDevice());
    }
}

void Faye::VkImageResource::wrapSwapchainImage(VulkanDevice &device,
                                               VkImage swapchainImage,
                                               VkFormat swapchainFormat,
                                               VkExtent2D swapchainExtent,
                                               VkImageAspectFlags swapchainAspectFlags,
                                               uint32_t swapchainMipLevels,
                                               VkImageLayout initialLayout,
                                               bool createDefaultView)
{
    wrapExternal(device,
                 VkImageWrapInfo{
                     swapchainImage,
                     {swapchainExtent.width, swapchainExtent.height, 1},
                     swapchainFormat,
                     initialLayout,
                     swapchainAspectFlags,
                     VK_IMAGE_VIEW_TYPE_2D,
                     swapchainMipLevels,
                     1},
                 createDefaultView);
}

void Faye::VkImageResource::destroy(VkDevice device)
{

    if (imageView != VK_NULL_HANDLE)
    {
        vkDestroyImageView(device, imageView, nullptr);
        imageView = VK_NULL_HANDLE;
    }
    if (ownsImage && image != VK_NULL_HANDLE && allocation != VK_NULL_HANDLE)
    {
        vmaDestroyImage(allocator, image, allocation);
        image = VK_NULL_HANDLE;
        allocation = VK_NULL_HANDLE;
        allocator = VK_NULL_HANDLE;
    }
    ownsImage = false;
}

VkImageView Faye::VkImageResource::createImageView(VkDevice device,
                                                   uint32_t baseMipLevel,
                                                   uint32_t levelCount,
                                                   uint32_t baseArrayLayer,
                                                   uint32_t layerCount)
{
    if (image == VK_NULL_HANDLE)
    {
        throw std::runtime_error("Cannot create an image view for a null image");
    }

    if (imageView != VK_NULL_HANDLE)
    {
        vkDestroyImageView(device, imageView, nullptr);
        imageView = VK_NULL_HANDLE;
    }

    const uint32_t resolvedLevelCount = levelCount == 0 ? (mipLevels > baseMipLevel ? mipLevels - baseMipLevel : 1) : levelCount;
    const uint32_t resolvedLayerCount = layerCount == 0 ? (arrayLayers > baseArrayLayer ? arrayLayers - baseArrayLayer : 1) : layerCount;

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = viewType;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = aspectFlags;
    viewInfo.subresourceRange.baseMipLevel = baseMipLevel;
    viewInfo.subresourceRange.levelCount = resolvedLevelCount;
    viewInfo.subresourceRange.baseArrayLayer = baseArrayLayer;
    viewInfo.subresourceRange.layerCount = resolvedLayerCount;

    if (vkCreateImageView(device, &viewInfo, nullptr, &imageView) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create VkImageResource image view");
    }

    return imageView;
}

void Faye::VkImageResource::recordTransition(VkCommandBuffer commandBuffer,
                                             VkImageLayout newLayout,
                                             VkPipelineStageFlags2 srcStageMask,
                                             VkPipelineStageFlags2 dstStageMask,
                                             VkAccessFlags2 srcAccessMask,
                                             VkAccessFlags2 dstAccessMask,
                                             uint32_t baseMipLevel,
                                             uint32_t levelCount,
                                             uint32_t baseArrayLayer,
                                             uint32_t layerCount)
{
    VkImageMemoryBarrier2 barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    barrier.srcStageMask = srcStageMask;   // now VK_PIPELINE_STAGE_2_* flags
    barrier.srcAccessMask = srcAccessMask; // now VK_ACCESS_2_* flags
    barrier.dstStageMask = dstStageMask;
    barrier.dstAccessMask = dstAccessMask;
    barrier.oldLayout = layout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = aspectFlags;
    barrier.subresourceRange.baseMipLevel = baseMipLevel;
    barrier.subresourceRange.levelCount = levelCount;
    barrier.subresourceRange.baseArrayLayer = baseArrayLayer;
    barrier.subresourceRange.layerCount = layerCount;

    VkDependencyInfo depInfo{};
    depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    depInfo.imageMemoryBarrierCount = 1;
    depInfo.pImageMemoryBarriers = &barrier;

    vkCmdPipelineBarrier2(
        commandBuffer,
        &depInfo);

    layout = newLayout;
}

void Faye::VkImageResource::transitionLayout(VulkanDevice &device,
                                             VkImageLayout newLayout,
                                             VkPipelineStageFlags2 srcStageMask,
                                             VkPipelineStageFlags2 dstStageMask,
                                             VkAccessFlags2 srcAccessMask,
                                             VkAccessFlags2 dstAccessMask,
                                             uint32_t baseMipLevel,
                                             uint32_t levelCount,
                                             uint32_t baseArrayLayer,
                                             uint32_t layerCount)
{
    if (image == VK_NULL_HANDLE)
    {
        throw std::runtime_error("Cannot transition layout for a null image");
    }

    const uint32_t resolvedLevelCount = levelCount == 0 ? (mipLevels > baseMipLevel ? mipLevels - baseMipLevel : 1) : levelCount;
    const uint32_t resolvedLayerCount = layerCount == 0 ? (arrayLayers > baseArrayLayer ? arrayLayers - baseArrayLayer : 1) : layerCount;

    VkCommandBuffer commandBuffer = device.beginSingleTimeCommands();

    recordTransition(commandBuffer,
                     newLayout,
                     srcStageMask,
                     dstStageMask,
                     srcAccessMask,
                     dstAccessMask,
                     baseMipLevel,
                     resolvedLevelCount,
                     baseArrayLayer,
                     resolvedLayerCount);

    device.endSingleTimeCommands(commandBuffer);
}

void Faye::VkImageResource::imageBarrier(VkCommandBuffer cmd, VkImage image, VkImageAspectFlags aspect,
                                         VkImageLayout oldLayout, VkImageLayout newLayout,
                                         VkPipelineStageFlags2 srcStage, VkPipelineStageFlags2 dstStage,
                                         VkAccessFlags2 srcAccess, VkAccessFlags2 dstAccess)
{
    VkImageMemoryBarrier2 b{};
    b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    b.srcStageMask = srcStage;
    b.srcAccessMask = srcAccess;
    b.dstStageMask = dstStage;
    b.dstAccessMask = dstAccess;
    b.oldLayout = oldLayout;
    b.newLayout = newLayout;
    b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.image = image;
    b.subresourceRange = {aspect, 0, 1, 0, 1};
    VkDependencyInfo dep{};
    dep.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dep.imageMemoryBarrierCount = 1;
    dep.pImageMemoryBarriers = &b;
    vkCmdPipelineBarrier2(cmd, &dep);
}

VkImageAspectFlags Faye::VkImageResource::defaultAspectFlagsForFormat(VkFormat format)
{
    if (isDepthFormat(format))
    {
        VkImageAspectFlags flags = VK_IMAGE_ASPECT_DEPTH_BIT;
        if (hasStencilComponent(format))
        {
            flags |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }
        return flags;
    }

    return VK_IMAGE_ASPECT_COLOR_BIT;
}