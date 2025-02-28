#pragma once

#include <vulkan/vulkan.h>

#include "Pipeline.hpp"

#include "vk_device.hpp"

namespace Faye {

    struct PipelineConfigInfo {
        PipelineConfigInfo() = default;
        PipelineConfigInfo(const PipelineConfigInfo&) = delete;
        PipelineConfigInfo& operator=(const PipelineConfigInfo&) = delete;

        VkPipelineViewportStateCreateInfo viewportInfo;
        VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo;
        VkPipelineRasterizationStateCreateInfo rasterizationInfo;
        VkPipelineMultisampleStateCreateInfo multisamplingInfo;
        VkPipelineColorBlendAttachmentState colorBlendAttachment;
        VkPipelineColorBlendStateCreateInfo colorBlendingInfo;
        VkPipelineDepthStencilStateCreateInfo depthStencilInfo;
        std::vector<VkDynamicState> dynamicStateEnables;
        VkPipelineDynamicStateCreateInfo dynamicStateInfo;
        VkPipelineLayout pipelineLayout = nullptr;
        VkRenderPass renderPass = nullptr;
        uint32_t subpass = 0;
    };

    class VulkanPipeline : Pipeline {
        public:
            VulkanPipeline(VulkanDevice &device, const std::string& vertFilepath, const std::string& fragFilepath, const PipelineConfigInfo &config);
            ~VulkanPipeline();

            VulkanPipeline(const VulkanPipeline &) = delete;
            VulkanPipeline& operator=(const VulkanPipeline &) = delete;

            void bind(VkCommandBuffer commandBuffer);

            static void defaultPipelineConfigInfo(PipelineConfigInfo &configInfo);

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