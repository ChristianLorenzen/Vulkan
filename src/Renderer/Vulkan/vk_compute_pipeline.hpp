#pragma once

#include <vulkan/vulkan.h>

#include "Pipeline.hpp"
#include "ShaderReflection.hpp"

#include "vk_device.hpp"
#include "vk_descriptors.hpp"

namespace Faye
{
    struct VulkanComputeBinding {
        uint32_t binding;
        VkDescriptorImageInfo *image = nullptr;
        VkDescriptorBufferInfo *buffer = nullptr;
    };

    class VulkanComputePipeline : Pipeline
    {
    public:
        VulkanComputePipeline(VulkanDevice &device, const std::string &compFilepath);
        ~VulkanComputePipeline();

        void dispatch(VkCommandBuffer cmd,
            uint32_t groupCountX,
            uint32_t groupCountY,
            uint32_t groupCountZ,
            const std::vector<VulkanComputeBinding> &bindings,
            const void *pushData = nullptr,
            uint32_t pushDataSize = 0
        );
    private:
        VulkanDevice &device;
        ShaderReflectionData reflection;
        std::unique_ptr<VulkanDescriptorSetLayout> setLayout;
        VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};
        VkPipeline computePipeline{VK_NULL_HANDLE};
    };
}