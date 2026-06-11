#include "vk_pipeline.hpp"
#include "Core/IO/FileSystem.hpp"
#include "Renderer/Resources/Vertex.hpp"

#include "quill/LogMacros.h"

using namespace Faye;

VulkanPipeline::VulkanPipeline(VulkanDevice &device, const std::string &vertFilepath, const std::string &fragFilepath, const PipelineConfigInfo &config) : device(device)
{
    LOG_INFO(Logger::getInstance(), "Creating Graphics Pipeline...");
    createGraphicsPipeline(vertFilepath, fragFilepath, config);
}

VulkanPipeline::~VulkanPipeline()
{
    if (graphicsPipeline != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(device.getDevice(), graphicsPipeline, nullptr);
    }
}

void VulkanPipeline::createGraphicsPipeline(const std::string &vertFilepath, const std::string &fragFilepath, const PipelineConfigInfo &config)
{
    auto vertShaderCode = FileSystem::readFile(vertFilepath);
    auto fragShaderCode = FileSystem::readFile(fragFilepath);

    VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
    VkShaderModule fragShaderModule{VK_NULL_HANDLE};

    try {
        fragShaderModule = createShaderModule(fragShaderCode);
    }
    catch (const std::exception &e)
    {
        vkDestroyShaderModule(device.getDevice(), vertShaderModule, nullptr);
        throw; // Re-throw the exception after cleanup
    }

    VkPipelineShaderStageCreateInfo vertShaderStageInfo = {};
    vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName = "main";
    vertShaderStageInfo.flags = 0;
    vertShaderStageInfo.pNext = nullptr;
    vertShaderStageInfo.pSpecializationInfo = nullptr;

    VkPipelineShaderStageCreateInfo fragShaderStageInfo = {};
    fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName = "main";
    fragShaderStageInfo.flags = 0;
    fragShaderStageInfo.pNext = nullptr;
    fragShaderStageInfo.pSpecializationInfo = nullptr;

    VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    auto &bindingDescriptions = config.bindingDescriptions;
    auto &attributeDescriptions = config.attributeDescriptions;
    auto &colorBlendAttachments = config.colorBlendAttachments;

    vertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(bindingDescriptions.size());
    vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexBindingDescriptions = bindingDescriptions.data();
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    VkGraphicsPipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &config.inputAssemblyInfo;
    pipelineInfo.pViewportState = &config.viewportInfo;
    pipelineInfo.pRasterizationState = &config.rasterizationInfo;
    pipelineInfo.pMultisampleState = &config.multisamplingInfo;
    pipelineInfo.pDepthStencilState = &config.depthStencilInfo;
    pipelineInfo.pDynamicState = &config.dynamicStateInfo;
    pipelineInfo.layout = config.pipelineLayout;
    pipelineInfo.renderPass = VK_NULL_HANDLE;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineInfo.basePipelineIndex = -1;

    VkPipelineColorBlendStateCreateInfo colorBlendingInfo = config.colorBlendingInfo;
    colorBlendingInfo.attachmentCount = static_cast<uint32_t>(colorBlendAttachments.size());
    colorBlendingInfo.pAttachments = colorBlendAttachments.data();
    pipelineInfo.pColorBlendState = &colorBlendingInfo;

    // Dynamic rendering: when no VkRenderPass is provided, the pipeline must declare
    // its attachment formats via VkPipelineRenderingCreateInfo. Without this chained,
    // the pipeline is created with zero attachments (depth format UNDEFINED), which
    // silently disables depth testing and blending state at draw time.
    VkPipelineRenderingCreateInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    renderingInfo.colorAttachmentCount = static_cast<uint32_t>(config.colorAttachmentFormats.size());
    renderingInfo.pColorAttachmentFormats = config.colorAttachmentFormats.empty() ? nullptr : config.colorAttachmentFormats.data();
    renderingInfo.depthAttachmentFormat = config.depthAttachmentFormat;
    renderingInfo.stencilAttachmentFormat = config.stencilAttachmentFormat;
    pipelineInfo.pNext = &renderingInfo;

    LOG_INFO(Logger::getInstance(), "Initialized Pipeline Info...");

    VkResult result = vkCreateGraphicsPipelines(device.getDevice(), device.getPipelineCache(), 1, &pipelineInfo, nullptr, &graphicsPipeline);
    
    vkDestroyShaderModule(device.getDevice(), fragShaderModule, nullptr);
    vkDestroyShaderModule(device.getDevice(), vertShaderModule, nullptr);

    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create graphics pipeline");
    }
}

VkShaderModule VulkanPipeline::createShaderModule(const std::vector<char> &code)
{
    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t *>(code.data());
    VkShaderModule shaderModule;
    if (vkCreateShaderModule(device.getDevice(), &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create shader module");
    }
    return shaderModule;
}

void VulkanPipeline::defaultPipelineConfigInfo(PipelineConfigInfo &configInfo)
{
    configInfo.inputAssemblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    configInfo.inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    configInfo.inputAssemblyInfo.primitiveRestartEnable = VK_FALSE;

    configInfo.viewportInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    configInfo.viewportInfo.viewportCount = 1;
    configInfo.viewportInfo.pViewports = nullptr;
    configInfo.viewportInfo.scissorCount = 1;
    configInfo.viewportInfo.pScissors = nullptr;

    configInfo.rasterizationInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    configInfo.rasterizationInfo.depthClampEnable = VK_FALSE;
    configInfo.rasterizationInfo.rasterizerDiscardEnable = VK_FALSE;
    configInfo.rasterizationInfo.polygonMode = VK_POLYGON_MODE_FILL;
    configInfo.rasterizationInfo.lineWidth = 1.0f;
    configInfo.rasterizationInfo.cullMode = VK_CULL_MODE_NONE;
    configInfo.rasterizationInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
    configInfo.rasterizationInfo.depthBiasEnable = VK_FALSE;
    configInfo.rasterizationInfo.depthBiasConstantFactor = 0.0f;
    configInfo.rasterizationInfo.depthBiasClamp = 0.0f;
    configInfo.rasterizationInfo.depthBiasSlopeFactor = 0.0f;

    configInfo.multisamplingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    configInfo.multisamplingInfo.sampleShadingEnable = VK_FALSE;
    configInfo.multisamplingInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    configInfo.multisamplingInfo.minSampleShading = 1.0f;
    configInfo.multisamplingInfo.pSampleMask = nullptr;
    configInfo.multisamplingInfo.alphaToCoverageEnable = VK_FALSE;
    configInfo.multisamplingInfo.alphaToOneEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState defaultAttachment{};
    defaultAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    defaultAttachment.blendEnable = VK_FALSE;
    defaultAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    defaultAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    defaultAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    defaultAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    defaultAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    defaultAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
    configInfo.colorBlendAttachments = {defaultAttachment};

    configInfo.colorBlendingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    configInfo.colorBlendingInfo.logicOpEnable = VK_FALSE;
    configInfo.colorBlendingInfo.logicOp = VK_LOGIC_OP_COPY;
    configInfo.colorBlendingInfo.attachmentCount = static_cast<uint32_t>(configInfo.colorBlendAttachments.size());
    configInfo.colorBlendingInfo.pAttachments = configInfo.colorBlendAttachments.data();
    configInfo.colorBlendingInfo.blendConstants[0] = 0.0f;
    configInfo.colorBlendingInfo.blendConstants[1] = 0.0f;
    configInfo.colorBlendingInfo.blendConstants[2] = 0.0f;
    configInfo.colorBlendingInfo.blendConstants[3] = 0.0f;

    configInfo.depthStencilInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    configInfo.depthStencilInfo.depthTestEnable = VK_TRUE;
    configInfo.depthStencilInfo.depthWriteEnable = VK_TRUE;
    configInfo.depthStencilInfo.depthCompareOp = VK_COMPARE_OP_LESS;
    configInfo.depthStencilInfo.depthBoundsTestEnable = VK_FALSE;
    configInfo.depthStencilInfo.stencilTestEnable = VK_FALSE;

    configInfo.dynamicStateEnables = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
        VK_DYNAMIC_STATE_CULL_MODE,
        VK_DYNAMIC_STATE_FRONT_FACE,
        VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY,
        VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE,
        VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE,
        VK_DYNAMIC_STATE_DEPTH_COMPARE_OP,
    };

    configInfo.dynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    configInfo.dynamicStateInfo.dynamicStateCount = static_cast<uint32_t>(configInfo.dynamicStateEnables.size());
    configInfo.dynamicStateInfo.pDynamicStates = configInfo.dynamicStateEnables.data();
    configInfo.dynamicStateInfo.flags = 0;

    configInfo.bindingDescriptions = Vertex::getBindingDescription();
    configInfo.attributeDescriptions = Vertex::getAttributeDescriptions();
}

void VulkanPipeline::bind(VkCommandBuffer commandBuffer)
{
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
}