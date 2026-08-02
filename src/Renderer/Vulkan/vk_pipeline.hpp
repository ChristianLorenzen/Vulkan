#pragma once

#include <vulkan/vulkan.h>

#include "Pipeline.hpp"

#include "vk_device.hpp"

namespace Faye
{

    struct PipelineConfigInfo
    {
        PipelineConfigInfo() = default;
        PipelineConfigInfo(const PipelineConfigInfo &) = delete;
        PipelineConfigInfo &operator=(const PipelineConfigInfo &) = delete;

        std::vector<VkVertexInputBindingDescription> bindingDescriptions;
        std::vector<VkVertexInputAttributeDescription> attributeDescriptions;
        VkPipelineViewportStateCreateInfo viewportInfo;
        VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo;
        VkPipelineRasterizationStateCreateInfo rasterizationInfo;
        VkPipelineMultisampleStateCreateInfo multisamplingInfo;
        std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachments;
        VkPipelineColorBlendStateCreateInfo colorBlendingInfo;
        VkPipelineDepthStencilStateCreateInfo depthStencilInfo;
        std::vector<VkDynamicState> dynamicStateEnables;
        VkPipelineDynamicStateCreateInfo dynamicStateInfo;
        VkPipelineTessellationStateCreateInfo tessellationInfo;
        VkPipelineLayout pipelineLayout = nullptr;

        std::vector<VkFormat> colorAttachmentFormats;
        VkFormat depthAttachmentFormat = VK_FORMAT_UNDEFINED;
        VkFormat stencilAttachmentFormat = VK_FORMAT_UNDEFINED;
    };

    class VulkanPipeline : Pipeline
    {
    public:
        // tescFilepath/tesePath are optional: when both are non-empty the pipeline
        // is built with a tessellation control + evaluation stage and
        // config.tessellationInfo is wired in as pTessellationState. Leave both
        // empty for a standard vertex+fragment pipeline (the default for every
        // existing caller).
        VulkanPipeline(VulkanDevice &device, const std::string &vertFilepath, const std::string &fragFilepath, const PipelineConfigInfo &config,
                       const std::string &tescFilepath = "", const std::string &tesePath = "");
        ~VulkanPipeline();

        VulkanPipeline(const VulkanPipeline &) = delete;
        VulkanPipeline &operator=(const VulkanPipeline &) = delete;

        void bind(VkCommandBuffer commandBuffer);

        static void defaultPipelineConfigInfo(PipelineConfigInfo &configInfo);

    private:
        void createGraphicsPipeline(const std::string &vertFilepath, const std::string &fragFilepath, const PipelineConfigInfo &config,
                                    const std::string &tescFilepath, const std::string &tesePath);

        VulkanDevice &device;
        VkPipeline graphicsPipeline{VK_NULL_HANDLE};
    };
    
    VkShaderModule createShaderModule(VulkanDevice &device, const std::vector<char> &code);
}