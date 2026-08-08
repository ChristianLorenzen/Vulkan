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
        parameterBuffer.reset();
        uniformData = {};
        albedoSlot = normalSlot = metallicSlot = roughnessSlot = aoSlot = 0;
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
                BufferMemoryUsage::HostVisible);
            state.parameterBuffer->map();
        }
        state.parameterBuffer->writeToBuffer(&state.uniformData);

        // Upload all textures into the bindless set and record slot indices.
        // Pre-populate with fallback slots so any missing texture uses the default.
        state.albedoSlot = textureCache.getFallbackSlot(TextureType::Albedo);
        state.normalSlot = textureCache.getFallbackSlot(TextureType::Normal);
        state.metallicSlot = textureCache.getFallbackSlot(TextureType::Metallic);
        state.roughnessSlot = textureCache.getFallbackSlot(TextureType::Roughness);
        state.aoSlot = textureCache.getFallbackSlot(TextureType::AmbientOcclusion);

        for (const Texture &texture : materialData.textures)
        {
            if (!texture.hasPixelData() || texture.width <= 0 || texture.height <= 0)
            {
                continue;
            }

            try
            {
                auto [resource, slot] = textureCache.getOrCreateTextureAndSlot(texture);
                (void)resource;
                switch (texture.type)
                {
                case TextureType::Albedo:
                    state.albedoSlot = slot;
                    break;
                case TextureType::Normal:
                    state.normalSlot = slot;
                    break;
                case TextureType::Metallic:
                    state.metallicSlot = slot;
                    break;
                case TextureType::Roughness:
                    state.roughnessSlot = slot;
                    break;
                case TextureType::AmbientOcclusion:
                    state.aoSlot = slot;
                    break;
                default:
                    break;
                }
            }
            catch (const std::exception &exception)
            {
                LOG_WARNING(
                    Logger::get(),
                    "Failed to upload texture '{}' for material '{}': {}. Falling back to default material texture.",
                    texture.path,
                    material.getName(),
                    exception.what());
            }
        }

        writeParamDescriptorSet(state);
    }

    void MaterialCache::writeParamDescriptorSet(MaterialState &state)
    {
        VkDescriptorBufferInfo parameterInfo = state.parameterBuffer->descriptorInfo();

        VulkanDescriptorWriter writer(materialSetLayout, materialPool);
        writer.writeBuffer(0, &parameterInfo);

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

}