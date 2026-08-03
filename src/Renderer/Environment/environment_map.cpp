#include "environment_map.hpp"
#include "Renderer/Material/TextureLoader.hpp"
#include "Renderer/Vulkan/TextureCache.hpp"
#include "Renderer/Vulkan/VulkanBuffer.hpp"
#include "Renderer/Vulkan/vk_device.hpp"
#include "Core/Logging/Logger.hpp"
#include "Core/Path/Paths.hpp"
#include "quill/LogMacros.h"

#include <stdexcept>

namespace Faye {
namespace {

    uint32_t mipCountFor(uint32_t size) {
        uint32_t levels = 1;
        while (size > 1) { size >>= 1; ++levels; }
        return levels;
    }

    VkImageResourceCreateInfo cubeCreateInfo(uint32_t faceSize, uint32_t mipLevels) {
        VkImageResourceCreateInfo info{};
        info.extent = {faceSize, faceSize, 1};
        info.format = VK_FORMAT_R16G16B16A16_SFLOAT;
        info.usage = VK_IMAGE_USAGE_STORAGE_BIT //compute writes
        | VK_IMAGE_USAGE_SAMPLED_BIT            // shaders read
        | VK_IMAGE_USAGE_TRANSFER_SRC_BIT       // blit source for mips
        | VK_IMAGE_USAGE_TRANSFER_DST_BIT;      // blit dest for mips
        info.aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
        info.imageType = VK_IMAGE_TYPE_2D;
        info.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
        info.arrayLayers = 6;
        info.mipLevels = mipLevels;
        info.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        return info;
    }

} // namespace

    void EnvironmentMap::createCubeImages(VulkanDevice &device) {
        skyMips = mipCountFor(kSkyFaceSize);
        skyCube.createOwned(device, cubeCreateInfo(kSkyFaceSize, skyMips), true);
        irradianceCube.createOwned(device, cubeCreateInfo(kIrradianceFaceSize, 1), true);
        prefilteredCube.createOwned(device, cubeCreateInfo(kPrefilteredFaceSize, prefilteredMips), true);

        VkSamplerResourceCreateInfo s{};
        s.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        s.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        s.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        s.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        s.enableAnisotropy = false;
        s.maxLod = static_cast<float>(skyMips);
        cubeSampler.create(device,s);
    }

    void EnvironmentMap::generateSkyMips(VulkanDevice& device) {
        VkCommandBuffer cmd = device.beginSingleTimeCommands();

        VkImageResource::imageBarrier(cmd, skyCube.image, VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_2_BLIT_BIT,
            VK_ACCESS_2_SHADER_WRITE_BIT, VK_ACCESS_2_TRANSFER_READ_BIT, 0, 1, 0, 6);

        int32_t srcSize = static_cast<int32_t>(kSkyFaceSize);
        for (uint32_t mip = 1; mip < skyMips; ++mip) {
            const int32_t dstSize = srcSize > 1 ? srcSize / 2 : 1;

            VkImageResource::imageBarrier(cmd, skyCube.image, VK_IMAGE_ASPECT_COLOR_BIT,
                VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_PIPELINE_STAGE_2_BLIT_BIT, VK_PIPELINE_STAGE_2_BLIT_BIT,
                0, VK_ACCESS_2_TRANSFER_WRITE_BIT, mip, 1, 0, 6);

            VkImageBlit blit{};
            blit.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, mip - 1, 0, 6};
            blit.srcOffsets[1] = {srcSize, srcSize, 1};
            blit.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, mip, 0, 6};
            blit.dstOffsets[1] = {dstSize, dstSize, 1};

            vkCmdBlitImage(cmd, skyCube.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            skyCube.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

            VkImageResource::imageBarrier(cmd, skyCube.image, VK_IMAGE_ASPECT_COLOR_BIT,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                VK_PIPELINE_STAGE_2_BLIT_BIT, VK_PIPELINE_STAGE_2_BLIT_BIT,
                VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_ACCESS_2_TRANSFER_READ_BIT,
                mip, 1, 0, 6);

            srcSize = dstSize;
        }

        VkImageResource::imageBarrier(cmd, skyCube.image, VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_2_BLIT_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
            VK_ACCESS_2_TRANSFER_READ_BIT, VK_ACCESS_2_SHADER_READ_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, 6);

        skyCube.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        device.endSingleTimeCommands(cmd);
    }

    void EnvironmentMap::bakeEquirectToCube(VulkanDevice& device, const VkImageResource &equirect, VkSampler equirectSampler) {
        VkCommandBuffer cmd = device.beginSingleTimeCommands();

        skyCube.recordTransition(cmd, VK_IMAGE_LAYOUT_GENERAL,
        VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        0, VK_ACCESS_2_SHADER_WRITE_BIT);

        VkImageView mip0 = skyCube.makeView(device.getDevice(), VK_IMAGE_VIEW_TYPE_CUBE, 0, 1, 0, 6);

        VkDescriptorImageInfo srcInfo{};
        srcInfo.sampler = equirectSampler;
        srcInfo.imageView = equirect.imageView;
        srcInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkDescriptorImageInfo dstInfo{};
        dstInfo.imageView = mip0;
        dstInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        struct { uint32_t faceSize; } push{kSkyFaceSize};
        const uint32_t groups = kSkyFaceSize / 8;
        equirectToCubePipeline->dispatch(cmd, groups, groups, 6, {{0, &srcInfo}, {1, &dstInfo}}, &push, sizeof(push));

        device.endSingleTimeCommands(cmd);
        vkDestroyImageView(device.getDevice(), mip0, nullptr);
    }

    void EnvironmentMap::bakeIrradiance(VulkanDevice & device)
    {
        VkCommandBuffer cmd = device.beginSingleTimeCommands();

        irradianceCube.recordTransition(cmd, VK_IMAGE_LAYOUT_GENERAL,
            VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            0, VK_ACCESS_2_SHADER_WRITE_BIT);

        VkImageView dstView = irradianceCube.makeView(device.getDevice(), VK_IMAGE_VIEW_TYPE_CUBE, 0, 1, 0, 6);

        VkDescriptorImageInfo srcInfo{cubeSampler.sampler, skyCube.imageView,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        VkDescriptorImageInfo dstInfo{VK_NULL_HANDLE, dstView,
        VK_IMAGE_LAYOUT_GENERAL};

        struct { uint32_t faceSize; float sampleDelta; } push{ kIrradianceFaceSize, 0.025f };
        const uint32_t groups = (kIrradianceFaceSize + 7) / 8;
        irradiancePipeline->dispatch(cmd, groups, groups, 
            6, {{0, &srcInfo}, {1, &dstInfo}}, &push, sizeof(push));
        
        irradianceCube.recordTransition(cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
            VK_ACCESS_2_SHADER_WRITE_BIT, VK_ACCESS_2_SHADER_READ_BIT);
        
        device.endSingleTimeCommands(cmd);
        vkDestroyImageView(device.getDevice(), dstView, nullptr);
    }

    void EnvironmentMap::bakePrefiltered(VulkanDevice & device)
    {
        VkCommandBuffer cmd = device.beginSingleTimeCommands();

        prefilteredCube.recordTransition(cmd, VK_IMAGE_LAYOUT_GENERAL,
            VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            0, VK_ACCESS_2_SHADER_WRITE_BIT);

        VkDescriptorImageInfo srcInfo{cubeSampler.sampler, skyCube.imageView,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};

        std::vector<VkImageView> mipViews(prefilteredMips, VK_NULL_HANDLE);
        for (uint32_t mip = 0; mip < prefilteredMips; ++mip) {
            const uint32_t mipSize = kPrefilteredFaceSize >> mip;
            mipViews[mip] = prefilteredCube.makeView(device.getDevice(), VK_IMAGE_VIEW_TYPE_CUBE, mip, 1, 0, 6);
            
            VkDescriptorImageInfo dstInfo{VK_NULL_HANDLE, mipViews[mip], VK_IMAGE_LAYOUT_GENERAL};
            struct { uint32_t faceSize; float roughness; uint32_t sampleCount; float skyFaceSize; } push{mipSize, static_cast<float>(mip) / static_cast<float>(prefilteredMips - 1), 256u, static_cast<float>(kSkyFaceSize)};
        
            const uint32_t groups = (mipSize + 7) / 8;
            prefilterPipeline->dispatch(cmd, groups, groups, 6, {{0, &srcInfo}, {1, &dstInfo}}, &push, sizeof(push));
        }
        prefilteredCube.recordTransition(cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
            VK_ACCESS_2_SHADER_WRITE_BIT, VK_ACCESS_2_SHADER_READ_BIT);
        
        device.endSingleTimeCommands(cmd);
        for (VkImageView view : mipViews) {
            vkDestroyImageView(device.getDevice(), view, nullptr);
        }
    }

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

        // The equirect is scratch: it exists only long enough for the conversion
        // dispatch to read it, then it is freed. At 4k RGBA32F that reclaims
        // ~134 MB, more than the entire cube chain costs.
        VkImageResource equirect;
        equirect.createOwned(
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

        equirect.transitionLayout(
            device,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            0,
            VK_ACCESS_2_TRANSFER_WRITE_BIT);

        device.copyBufferToImage(
            stagingBuffer.getBuffer(),
            equirect.image,
            static_cast<uint32_t>(im.width),
            static_cast<uint32_t>(im.height));

        // Destination stage is COMPUTE, not FRAGMENT: the only reader is the
        // conversion dispatch below.
        equirect.transitionLayout(
            device,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
            VK_ACCESS_2_TRANSFER_WRITE_BIT,
            VK_ACCESS_2_SHADER_READ_BIT);

        // U wraps because azimuth is periodic; V clamps because the poles are the
        // first/last texel rows and must not bleed into each other.
        VkSamplerResource equirectSampler;
        VkSamplerResourceCreateInfo samplerInfo{};
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.enableAnisotropy = false;
        equirectSampler.create(device, samplerInfo);

        createCubeImages(device);

        
        equirectToCubePipeline = std::make_unique<VulkanComputePipeline>(device, Paths::compiledShader("ibl_equirect_to_cube.comp").string());
        irradiancePipeline = std::make_unique<VulkanComputePipeline>(device, Paths::compiledShader("ibl_irradiance.comp").string());
        prefilterPipeline = std::make_unique<VulkanComputePipeline>(device, Paths::compiledShader("ibl_prefilter.comp").string());

        bakeEquirectToCube(device, equirect, equirectSampler.sampler);
        generateSkyMips(device);

        bakeIrradiance(device);
        bakePrefiltered(device);

        // endSingleTimeCommands submits and waits, so the bake is complete here
        // and the scratch resources are safe to release.
        equirectSampler.destroy(device.getDevice());
        equirect.destroy(device.getDevice());

        LOG_INFO(Logger::get(),
                 "EnvironmentMap: baked {}x{} HDR from {} into a {}x{} cubemap",
                 im.width, im.height, hdrPath, kSkyFaceSize, kSkyFaceSize);
        return true;
    }

    void EnvironmentMap::destroy(VkDevice device) {
        cubeSampler.destroy(device);
        skyCube.destroy(device);
        irradianceCube.destroy(device);
        prefilteredCube.destroy(device);
    }

    bool EnvironmentMap::isValid() const {
        return skyCube.isValid() && skyCube.hasView() && cubeSampler.isValid();
    }

    VkDescriptorImageInfo EnvironmentMap::skyInfo() const {
        return cubeSampler.descriptorInfo(skyCube.imageView);
    }

    VkDescriptorImageInfo EnvironmentMap::irradianceInfo() const {
        return cubeSampler.descriptorInfo(irradianceCube.imageView);
    }

    VkDescriptorImageInfo EnvironmentMap::prefilteredInfo() const {
        return cubeSampler.descriptorInfo(prefilteredCube.imageView);
    }
}