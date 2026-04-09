#include "MaterialCache.hpp"

#include <array>
#include <stdexcept>
#include <utility>

#include "Core/Logging/Logger.hpp"
#include "quill/LogMacros.h"

namespace Faye
{
    void MaterialState::reset()
    {
        handle = {};
        pipelineConfig = {};
        descriptorSet = VK_NULL_HANDLE;
        textures.clear();
    }

    const VkTextureResource *MaterialState::findTexture(TextureType type) const
    {
        const auto iterator = textures.find(type);
        return iterator != textures.end() ? &iterator->second : nullptr;
    }

    MaterialCache::MaterialCache(VulkanDevice &device,
                                 VulkanDescriptorSetLayout &materialSetLayout,
                                 VulkanDescriptorPool &materialPool)
        : vk_device(device), materialSetLayout(materialSetLayout), materialPool(materialPool)
    {
        ensureFallbackResources();
    }

    MaterialCache::~MaterialCache()
    {
        clear();
        fallbackAlbedoTexture.destroy(vk_device.getDevice());
    }

    const MaterialState &MaterialCache::getOrCreateState(MaterialHandle handle, const Material &material)
    {
        if (!handle.isValid())
        {
            throw std::runtime_error("Cannot build MaterialState for an invalid handle");
        }

        auto [iterator, inserted] = materialStates.try_emplace(handle.value);
        if (inserted)
        {
            buildState(iterator->second, handle, material);
        }

        return iterator->second;
    }

    const MaterialState *MaterialCache::findState(MaterialHandle handle) const
    {
        const auto iterator = materialStates.find(handle.value);
        return iterator != materialStates.end() ? &iterator->second : nullptr;
    }

    void MaterialCache::invalidateMaterial(MaterialHandle handle)
    {
        const auto iterator = materialStates.find(handle.value);
        if (iterator == materialStates.end())
        {
            return;
        }

        destroyState(iterator->second);
        materialStates.erase(iterator);
    }

    void MaterialCache::clear()
    {
        for (auto &[handleValue, state] : materialStates)
        {
            (void)handleValue;
            destroyState(state);
        }
        materialStates.clear();
    }

    void MaterialCache::ensureFallbackResources()
    {
        if (fallbackAlbedoTexture.isValid())
        {
            return;
        }

        static const std::array<unsigned char, 4> kWhitePixel{255, 255, 255, 255};
        fallbackAlbedoTexture = createTextureResource(
            std::vector<unsigned char>(kWhitePixel.begin(), kWhitePixel.end()),
            1,
            1,
            TextureType::Albedo);
    }

    void MaterialCache::buildState(MaterialState &state, MaterialHandle handle, const Material &material)
    {
        ensureFallbackResources();

        state.reset();
        state.handle = handle;
        state.pipelineConfig = material.getPipelineConfig();

        const MaterialData &materialData = material.getMaterialData();
        for (const Texture &texture : materialData.textures)
        {
            if (texture.data.empty() || texture.width <= 0 || texture.height <= 0)
            {
                continue;
            }

            if (state.textures.contains(texture.type))
            {
                continue;
            }

            try
            {
                state.textures.emplace(texture.type, createTextureResource(texture));
            }
            catch (const std::exception &exception)
            {
                LOG_WARNING(
                    Logger::getInstance(),
                    "Failed to upload texture '{}' for material '{}': {}. Falling back to default material texture.",
                    texture.path,
                    material.getName(),
                    exception.what());
            }
        }

        writeDescriptorSet(state);
    }

    void MaterialCache::writeDescriptorSet(MaterialState &state)
    {
        const VkTextureResource &boundTexture = resolveBoundTexture(state);
        VkDescriptorImageInfo imageInfo = boundTexture.descriptorInfo();

        VulkanDescriptorWriter writer(materialSetLayout, materialPool);
        writer.writeImage(0, &imageInfo);

        if (!writer.build(state.descriptorSet))
        {
            throw std::runtime_error("Failed to allocate descriptor set for material state");
        }
    }

    void MaterialCache::destroyState(MaterialState &state)
    {
        if (state.descriptorSet != VK_NULL_HANDLE)
        {
            std::vector<VkDescriptorSet> descriptorSets{state.descriptorSet};
            materialPool.freeDescriptors(descriptorSets);
            state.descriptorSet = VK_NULL_HANDLE;
        }

        for (auto &[textureType, texture] : state.textures)
        {
            (void)textureType;
            texture.destroy(vk_device.getDevice());
        }

        state.reset();
    }

    VkTextureResource MaterialCache::createTextureResource(const Texture &texture) const
    {
        return createTextureResource(
            texture.data,
            static_cast<uint32_t>(texture.width),
            static_cast<uint32_t>(texture.height),
            texture.type);
    }

    VkTextureResource MaterialCache::createTextureResource(const std::vector<unsigned char> &pixelData,
                                                           uint32_t width,
                                                           uint32_t height,
                                                           TextureType type) const
    {
        if (pixelData.empty() || width == 0 || height == 0)
        {
            throw std::runtime_error("MaterialCache cannot upload an empty texture");
        }

        VulkanBuffer stagingBuffer(
            vk_device,
            pixelData.size(),
            1,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        stagingBuffer.map();
        stagingBuffer.writeToBuffer(const_cast<unsigned char *>(pixelData.data()));

        VkTextureResource textureResource{};
        textureResource.imageResource.createOwned(
            vk_device,
            VkImageResourceCreateInfo{
                {width, height, 1},
                resolveTextureFormat(type),
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

        textureResource.imageResource.transitionLayout(
            vk_device,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            VK_ACCESS_TRANSFER_WRITE_BIT);

        vk_device.copyBufferToImage(
            stagingBuffer.getBuffer(),
            textureResource.imageResource.image,
            width,
            height);

        textureResource.imageResource.transitionLayout(
            vk_device,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT);

        textureResource.samplerResource.create(vk_device);
        return textureResource;
    }

    const VkTextureResource &MaterialCache::resolveBoundTexture(const MaterialState &state) const
    {
        if (const VkTextureResource *albedoTexture = state.findTexture(TextureType::Albedo))
        {
            return *albedoTexture;
        }

        return fallbackAlbedoTexture;
    }

    const Texture *MaterialCache::selectTexture(const MaterialData &materialData, TextureType type)
    {
        for (const Texture &texture : materialData.textures)
        {
            if (texture.type == type)
            {
                return &texture;
            }
        }

        return nullptr;
    }

    VkFormat MaterialCache::resolveTextureFormat(TextureType type)
    {
        switch (type)
        {
        case TextureType::Albedo:
            return VK_FORMAT_R8G8B8A8_SRGB;
        case TextureType::Normal:
        case TextureType::Metallic:
        case TextureType::Roughness:
        case TextureType::AmbientOcclusion:
        case TextureType::Height:
            return VK_FORMAT_R8G8B8A8_UNORM;
        }

        return VK_FORMAT_R8G8B8A8_UNORM;
    }
}