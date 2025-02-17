#pragma once

#include <vulkan/vulkan.h>

#include "Pipeline.hpp"

#include "vk_device.hpp"

namespace Faye {

    struct PipelineConfigInfo {
        VkViewport viewport;
        VkRect2D scissor;
        VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo;
        VkPipelineRasterizationStateCreateInfo rasterizationInfo;
        VkPipelineMultisampleStateCreateInfo multisamplingInfo;
        VkPipelineColorBlendAttachmentState colorBlendAttachment;
        VkPipelineColorBlendStateCreateInfo colorBlendingInfo;
        VkPipelineDepthStencilStateCreateInfo depthStencilInfo;
        VkPipelineLayout pipelineLayout = nullptr;
        VkRenderPass renderPass = nullptr;
        uint32_t subpass = 0;
    };

    class VulkanPipeline : Pipeline {
        public:
            VulkanPipeline(VulkanDevice &device, const std::string& vertFilepath, const std::string& fragFilepath, const PipelineConfigInfo &config);
            ~VulkanPipeline();

            VulkanPipeline(const VulkanPipeline &) = delete;
            void operator=(const VulkanPipeline &) = delete;


            static PipelineConfigInfo defaultPipelineConfigInfo(uint32_t width, uint32_t height);
        private:
            void createGraphicsPipeline(const std::string& vertFilepath, const std::string& fragFilepath, const PipelineConfigInfo &config);

            VkShaderModule createShaderModule(const std::vector<char> &code);
            // Dangling references to the Vulkan device.
            // Only done as this should exist as long as this pipeline does.
            VulkanDevice& device;
            VkPipeline graphicsPipeline;
            VkShaderModule vertShaderModule;
            VkShaderModule fragShaderModule;
    };
}