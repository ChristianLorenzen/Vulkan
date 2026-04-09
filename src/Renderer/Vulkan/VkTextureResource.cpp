#include "VkTextureResource.hpp"

#include <stdexcept>
#include <utility>

Faye::VkTextureResource::VkTextureResource(VkTextureResource &&other) noexcept
{
    *this = std::move(other);
}

Faye::VkTextureResource &Faye::VkTextureResource::operator=(VkTextureResource &&other) noexcept
{
    if (this == &other)
    {
        return *this;
    }

    imageResource = std::move(other.imageResource);
    samplerResource = std::move(other.samplerResource);
    descriptorSet = std::exchange(other.descriptorSet, VK_NULL_HANDLE);
    return *this;
}

bool Faye::VkTextureResource::isValid() const
{
    return imageResource.isValid() && samplerResource.isValid();
}

void Faye::VkTextureResource::reset()
{
    imageResource.reset();
    samplerResource.reset();
    descriptorSet = VK_NULL_HANDLE;
}

void Faye::VkTextureResource::destroy(VkDevice device)
{
    samplerResource.destroy(device);
    imageResource.destroy(device);
    descriptorSet = VK_NULL_HANDLE;
}

VkDescriptorImageInfo Faye::VkTextureResource::descriptorInfo(VkImageLayout imageLayout) const
{
    if (!isValid())
    {
        throw std::runtime_error("Cannot create descriptor info from an incomplete VkTextureResource");
    }

    return samplerResource.descriptorInfo(imageResource.imageView, imageLayout);
}