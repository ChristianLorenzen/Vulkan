#include "environment_map.hpp"
#include "Renderer/Material/TextureLoader.hpp"
#include "Renderer/Vulkan/TextureCache.hpp"
#include "Renderer/Vulkan/VulkanBuffer.hpp"
#include "Renderer/Vulkan/vk_device.hpp"
#include "Core/Logging/Logger.hpp"
#include "quill/LogMacros.h"

#include <stdexcept>

namespace Faye {
    bool EnvironmentMap::load(VulkanDevice& device, const std::string &hdrPath) {

        HdrImage im{};
        try
        {
            im = loadHDRTextureFromFile(hdrPath, TextureType::Equirectangular);
        }
        catch (const std::exception &e)
        {
            LOG_ERROR(Logger::get(), "EnvironmentMap: {} -- skybox disabled.", e.what());
            return false;
        }

        if (im.pixels.empty() || im.width == 0 || im.height == 0)
        {
            LOG_ERROR(Logger::get(), "EnvironmentMap: HDR at {} decoded empty -- skybox disabled.", hdrPath);
            return false;
        }

        VulkanBuffer stagingBuffer(
            device,
            im.pixels.size() * sizeof(float),
            1,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        stagingBuffer.map();
        stagingBuffer.writeToBuffer(im.pixels.data());

        resource.createOwned(
            device,
            VkImageResourceCreateInfo{
                {static_cast<uint32_t>(im.width), static_cast<uint32_t>(im.height), 1},
                VK_FORMAT_R32G32B32A32_SFLOAT,
                VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                VK_IMAGE_ASPECT_COLOR_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_LAYOUT_UNDEFINED,
                VK_IMAGE_TYPE_2D,
                VK_IMAGE_VIEW_TYPE_2D,
                VK_SAMPLE_COUNT_1_BIT,
                1,
                1,
                0},
            true);

        resource.transitionLayout(
            device,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            0,
            VK_ACCESS_2_TRANSFER_WRITE_BIT);

        device.copyBufferToImage(
            stagingBuffer.getBuffer(),
            resource.image,
            static_cast<uint32_t>(im.width),
            static_cast<uint32_t>(im.height));

        resource.transitionLayout(
            device,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
            VK_ACCESS_2_TRANSFER_WRITE_BIT,
            VK_ACCESS_2_SHADER_READ_BIT);

        // U wraps because azimuth is periodic; V clamps because the poles are the
        // first/last texel rows and must not bleed into each other.
        VkSamplerResourceCreateInfo samplerInfo{};
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.enableAnisotropy = false;
        samplerResource.create(device, samplerInfo);

        LOG_INFO(Logger::get(), "EnvironmentMap: loaded {}x{} HDR environment from {}", im.width, im.height, hdrPath);
        return true;
    }

    void EnvironmentMap::destroy(VkDevice device) {
        samplerResource.destroy(device);
        resource.destroy(device);
    }

    bool EnvironmentMap::isValid() const {
        return resource.isValid() && resource.hasView() && samplerResource.isValid();
    }

    VkDescriptorImageInfo EnvironmentMap::descriptorInfo() const {
        return samplerResource.descriptorInfo(resource.imageView);
    }
}