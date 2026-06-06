#include "VkSamplerResource.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

Faye::VkSamplerResource::VkSamplerResource(VkSamplerResource &&other) noexcept
{
    *this = std::move(other);
}

Faye::VkSamplerResource &Faye::VkSamplerResource::operator=(VkSamplerResource &&other) noexcept
{
    if (this == &other)
    {
        return *this;
    }

    sampler = std::exchange(other.sampler, VK_NULL_HANDLE);
    return *this;
}

bool Faye::VkSamplerResource::isValid() const
{
    return sampler != VK_NULL_HANDLE;
}

void Faye::VkSamplerResource::reset()
{
    sampler = VK_NULL_HANDLE;
}

void Faye::VkSamplerResource::create(VulkanDevice &device, const VkSamplerResourceCreateInfo &createInfo)
{
    destroy(device.getDevice());

    VkSamplerCreateInfo samplerCreateInfo{};
    samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerCreateInfo.magFilter = createInfo.magFilter;
    samplerCreateInfo.minFilter = createInfo.minFilter;
    samplerCreateInfo.mipmapMode = createInfo.mipmapMode;
    samplerCreateInfo.addressModeU = createInfo.addressModeU;
    samplerCreateInfo.addressModeV = createInfo.addressModeV;
    samplerCreateInfo.addressModeW = createInfo.addressModeW;
    samplerCreateInfo.borderColor = createInfo.borderColor;
    samplerCreateInfo.unnormalizedCoordinates = createInfo.unnormalizedCoordinates;
    samplerCreateInfo.compareEnable = createInfo.compareEnable;
    samplerCreateInfo.compareOp = createInfo.compareOp;
    samplerCreateInfo.mipLodBias = createInfo.mipLodBias;
    samplerCreateInfo.minLod = createInfo.minLod;
    samplerCreateInfo.maxLod = createInfo.maxLod;

    if (createInfo.enableAnisotropy)
    {
        const float deviceMax = device.properties.limits.maxSamplerAnisotropy;
        const float requestedMax = createInfo.maxAnisotropy > 0.0f ? createInfo.maxAnisotropy : deviceMax;
        samplerCreateInfo.anisotropyEnable = VK_TRUE;
        samplerCreateInfo.maxAnisotropy = std::min(deviceMax, requestedMax);
    }
    else
    {
        samplerCreateInfo.anisotropyEnable = VK_FALSE;
        samplerCreateInfo.maxAnisotropy = 1.0f;
    }

    if (vkCreateSampler(device.getDevice(), &samplerCreateInfo, nullptr, &sampler) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create VkSamplerResource sampler");
    }
}

void Faye::VkSamplerResource::destroy(VkDevice device)
{
    if (sampler != VK_NULL_HANDLE)
    {
        vkDestroySampler(device, sampler, nullptr);
    }
    reset();
}

VkDescriptorImageInfo Faye::VkSamplerResource::descriptorInfo(VkImageView imageView, VkImageLayout imageLayout) const
{
    if (sampler == VK_NULL_HANDLE)
    {
        throw std::runtime_error("Cannot create descriptor info from a null sampler");
    }
    if (imageView == VK_NULL_HANDLE)
    {
        throw std::runtime_error("Cannot create descriptor info with a null image view");
    }

    VkDescriptorImageInfo imageInfo{};
    imageInfo.sampler = sampler;
    imageInfo.imageView = imageView;
    imageInfo.imageLayout = imageLayout;
    return imageInfo;
}