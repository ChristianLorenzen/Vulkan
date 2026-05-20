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
    memory = std::exchange(other.memory, VK_NULL_HANDLE);
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
    memory = VK_NULL_HANDLE;
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
    destroy(device.getDevice());

    if (createInfo.extent.width == 0 || createInfo.extent.height == 0 || createInfo.extent.depth == 0)
    {
        throw std::runtime_error("VkImageResource requires a non-zero extent");
    }
    if (createInfo.format == VK_FORMAT_UNDEFINED)
    {
        throw std::runtime_error("VkImageResource requires a valid VkFormat");
    }
    if (createInfo.usage == 0)
    {
        throw std::runtime_error("VkImageResource requires at least one usage flag");
    }

    extent = createInfo.extent;
    format = createInfo.format;
    layout = createInfo.initialLayout;
    aspectFlags = createInfo.aspectFlags != 0 ? createInfo.aspectFlags : defaultAspectFlagsForFormat(createInfo.format);
    viewType = createInfo.viewType;
    mipLevels = createInfo.mipLevels;
    arrayLayers = createInfo.arrayLayers;
    ownsImage = true;

    VkImageCreateInfo imageCreateInfo{};
    imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfo.flags = createInfo.flags;
    imageCreateInfo.imageType = createInfo.imageType;
    imageCreateInfo.extent = createInfo.extent;
    imageCreateInfo.mipLevels = createInfo.mipLevels;
    imageCreateInfo.arrayLayers = createInfo.arrayLayers;
    imageCreateInfo.format = createInfo.format;
    imageCreateInfo.tiling = createInfo.tiling;
    imageCreateInfo.initialLayout = createInfo.initialLayout;
    imageCreateInfo.usage = createInfo.usage;
    imageCreateInfo.samples = createInfo.samples;
    imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    device.createImageWithInfo(imageCreateInfo, createInfo.memoryProperties, image, memory);

    if (createDefaultView)
    {
        createImageView(device.getDevice());
    }
}

void Faye::VkImageResource::wrapExternal(VkDevice device,
                                         const VkImageWrapInfo &wrapInfo,
                                         bool createDefaultView)
{
    destroy(device);

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
    memory = VK_NULL_HANDLE;

    if (createDefaultView)
    {
        createImageView(device);
    }
}

void Faye::VkImageResource::wrapSwapchainImage(VkDevice device,
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
    }
    if (ownsImage && image != VK_NULL_HANDLE)
    {
        vkDestroyImage(device, image, nullptr);
    }
    if (ownsImage && memory != VK_NULL_HANDLE)
    {
        vkFreeMemory(device, memory, nullptr);
    }

    reset();
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

void Faye::VkImageResource::transitionLayout(VulkanDevice &device,
                                             VkImageLayout newLayout,
                                             VkPipelineStageFlags srcStageMask,
                                             VkPipelineStageFlags dstStageMask,
                                             VkAccessFlags srcAccessMask,
                                             VkAccessFlags dstAccessMask,
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

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = layout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = aspectFlags;
    barrier.subresourceRange.baseMipLevel = baseMipLevel;
    barrier.subresourceRange.levelCount = resolvedLevelCount;
    barrier.subresourceRange.baseArrayLayer = baseArrayLayer;
    barrier.subresourceRange.layerCount = resolvedLayerCount;
    barrier.srcAccessMask = srcAccessMask;
    barrier.dstAccessMask = dstAccessMask;

    vkCmdPipelineBarrier(
        commandBuffer,
        srcStageMask,
        dstStageMask,
        0,
        0,
        nullptr,
        0,
        nullptr,
        1,
        &barrier);

    device.endSingleTimeCommands(commandBuffer);
    layout = newLayout;
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