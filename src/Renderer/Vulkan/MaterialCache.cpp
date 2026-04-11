#include "MaterialCache.hpp"

#include <stdexcept>
#include <utility>

#include "Core/Logging/Logger.hpp"
#include "quill/LogMacros.h"

namespace Faye
{
    void MaterialState::reset()
    {
        handle = {};
        materialRevision = 0;
        pipelineConfig = {};
        descriptorSet = VK_NULL_HANDLE;
        textures.clear();
        parameterBuffer.reset();
        uniformData = {};
    }

    const VkTextureResource *MaterialState::findTexture(TextureType type) const
    {
        const auto iterator = textures.find(type);
        return iterator != textures.end() ? iterator->second.get() : nullptr;
    }

    MaterialCache::MaterialCache(VulkanDevice &device,
                                 TextureCache &textureCache,
                                 VulkanDescriptorSetLayout &materialSetLayout,
                                 VulkanDescriptorPool &materialPool)
        : vk_device(device), textureCache(textureCache), materialSetLayout(materialSetLayout), materialPool(materialPool)
    {
    }

    MaterialCache::~MaterialCache()
    {
        clear();
    }

    const MaterialState &MaterialCache::getOrCreateState(MaterialHandle handle, const Material &material)
    {
        if (!handle.isValid())
        {
            throw std::runtime_error("Cannot build MaterialState for an invalid handle");
        }

        auto [iterator, inserted] = materialStates.try_emplace(handle.value);
        if (inserted || iterator->second.materialRevision != material.getRevision())
        {
            refreshState(iterator->second, handle, material);
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

    void MaterialCache::refreshState(MaterialState &state, MaterialHandle handle, const Material &material)
    {
        state.handle = handle;
        state.materialRevision = material.getRevision();
        state.pipelineConfig = material.getPipelineConfig();

        const MaterialData &materialData = material.getMaterialData();
        state.uniformData.baseColorFactor = materialData.baseColorFactor;
        state.uniformData.baseColorFactor.a = materialData.opacity;
        state.uniformData.surfaceFactors = {
            materialData.metallicFactor,
            materialData.roughnessFactor,
            materialData.normalScale,
            materialData.occlusionStrength};
        state.uniformData.specularShininess = {
            materialData.specular.r * materialData.specularStrength,
            materialData.specular.g * materialData.specularStrength,
            materialData.specular.b * materialData.specularStrength,
            materialData.shininess};
        state.uniformData.emissiveFactors = {
            materialData.emissive.r,
            materialData.emissive.g,
            materialData.emissive.b,
            materialData.emissiveIntensity};
        state.uniformData.alphaModeCutoff = {
            static_cast<float>(materialData.alphaMode == MaterialAlphaMode::Mask ? 1.0f : 0.0f),
            materialData.alphaCutoff,
            0.0f,
            0.0f};

        if (state.parameterBuffer == nullptr)
        {
            state.parameterBuffer = std::make_unique<VulkanBuffer>(
                vk_device,
                sizeof(MaterialUniformData),
                1,
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            state.parameterBuffer->map();
        }
        state.parameterBuffer->writeToBuffer(&state.uniformData);

        state.textures.clear();
        for (const Texture &texture : materialData.textures)
        {
            if (!texture.hasPixelData() || texture.width <= 0 || texture.height <= 0)
            {
                continue;
            }

            try
            {
                state.textures.insert_or_assign(texture.type, textureCache.getOrCreateTexture(texture));
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
        VkDescriptorImageInfo albedoInfo = resolveBoundTexture(state, TextureType::Albedo).descriptorInfo();
        VkDescriptorImageInfo normalInfo = resolveBoundTexture(state, TextureType::Normal).descriptorInfo();
        VkDescriptorImageInfo metallicInfo = resolveBoundTexture(state, TextureType::Metallic).descriptorInfo();
        VkDescriptorImageInfo roughnessInfo = resolveBoundTexture(state, TextureType::Roughness).descriptorInfo();
        VkDescriptorImageInfo aoInfo = resolveBoundTexture(state, TextureType::AmbientOcclusion).descriptorInfo();
        VkDescriptorBufferInfo parameterInfo = state.parameterBuffer->descriptorInfo();

        VulkanDescriptorWriter writer(materialSetLayout, materialPool);
        writer.writeImage(0, &albedoInfo)
            .writeImage(1, &normalInfo)
            .writeImage(2, &metallicInfo)
            .writeImage(3, &roughnessInfo)
            .writeImage(4, &aoInfo)
            .writeBuffer(5, &parameterInfo);

        if (state.descriptorSet == VK_NULL_HANDLE)
        {
            if (!writer.build(state.descriptorSet))
            {
                throw std::runtime_error("Failed to allocate descriptor set for material state");
            }
            return;
        }

        writer.overwrite(state.descriptorSet);
    }

    void MaterialCache::destroyState(MaterialState &state)
    {
        if (state.descriptorSet != VK_NULL_HANDLE)
        {
            std::vector<VkDescriptorSet> descriptorSets{state.descriptorSet};
            materialPool.freeDescriptors(descriptorSets);
            state.descriptorSet = VK_NULL_HANDLE;
        }

        state.reset();
    }

    const VkTextureResource &MaterialCache::resolveBoundTexture(const MaterialState &state, TextureType type) const
    {
        if (const VkTextureResource *texture = state.findTexture(type))
        {
            return *texture;
        }

        return textureCache.getFallbackTexture(type);
    }
}