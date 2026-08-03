#pragma once

#include <vector>
#include <string>

#include <vulkan/vulkan.h>

#include "Renderer/Vulkan/vk_compute_pipeline.hpp"
#include "Renderer/Vulkan/vk_descriptors.hpp"
#include "Renderer/Vulkan/VkImageResource.hpp"
#include "Renderer/Vulkan/VkSamplerResource.hpp"
#include "Renderer/Material/Material.hpp"


namespace Faye {
    class EnvironmentMap {
    public:
        bool load(VulkanDevice&, const std::string &hdrPath);
        void destroy(VkDevice device);
        bool isValid() const;

        VkDescriptorImageInfo skyInfo() const;
        VkDescriptorImageInfo irradianceInfo() const;
        VkDescriptorImageInfo prefilteredInfo() const;

        uint32_t prefilteredMipCount() const { return prefilteredMips; }

        //VkDescriptorImageInfo descriptorInfo() const;
    private:
        void createCubeImages(VulkanDevice &device);
        void bakeEquirectToCube(VulkanDevice &device, const VkImageResource &equirectImage, VkSampler sampler);
        void generateSkyMips(VulkanDevice &device);
        void bakeIrradiance(VulkanDevice &device);
        void bakePrefiltered(VulkanDevice &device);

        VkImageResource skyCube; // faceSize^2 x 6, full mip chain
        VkImageResource irradianceCube; // 32^2 x 6, 1 mip
        VkImageResource prefilteredCube; // 128^2 x 6

        VkSamplerResource cubeSampler;

        std::unique_ptr<VulkanComputePipeline> equirectToCubePipeline;
        std::unique_ptr<VulkanComputePipeline> irradiancePipeline;
        std::unique_ptr<VulkanComputePipeline> prefilterPipeline;

        static constexpr uint32_t kSkyFaceSize = 1024;
        static constexpr uint32_t kIrradianceFaceSize = 32;
        static constexpr uint32_t kPrefilteredFaceSize = 128;
        uint32_t skyMips = 0;
        uint32_t prefilteredMips = 5;

        // OLD
        // VkImageResource resource;
        // VkSamplerResource samplerResource;
    };
}