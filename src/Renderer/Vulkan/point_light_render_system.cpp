#define VK_USER_PLATFORM_MACOS_MVK
#include <vulkan/vulkan.h>
#include <stdio.h>
#include <unordered_map>

#include "Vulkan.hpp"
#include "point_light_render_system.hpp"
#include "Core/Path/Paths.hpp"

#include "quill/LogMacros.h"

#include <chrono>

using namespace Faye;

struct PointLightPushConstantData
{
    glm::vec4 position{};
    glm::vec4 color{};
    float radius;
};

Faye::PointLightRenderSystem::PointLightRenderSystem(VulkanDevice &device, VkFormat colorFormat, VkFormat motionFormat, VkFormat depthFormat, VulkanDescriptorSetLayout &globalSetLayout) : vk_device(device), globalDescriptorSetLayout(globalSetLayout)
{
    LOG_INFO(Logger::get(), "Creating Vulkan Pipeline Layout...");
    createPipelineLayout(globalSetLayout.getDescriptorSetLayout());
    createPipeline(colorFormat, motionFormat, depthFormat);
}

Faye::PointLightRenderSystem::~PointLightRenderSystem()
{
    vkDestroyPipelineLayout(vk_device.getDevice(), pipelineLayout, nullptr);
}

// ------------------------------------- Conversion Functions ------------------------------------- //
void Faye::PointLightRenderSystem::createPipelineLayout(VkDescriptorSetLayout globalSetLayout)
{
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(PointLightPushConstantData);

    std::vector<VkDescriptorSetLayout> descriptorSetLayouts{globalSetLayout};

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = (uint32_t)descriptorSetLayouts.size();
    pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

    LOG_INFO(Logger::get(), "Created pipelinelayoutinfo struct...");

    if (vkCreatePipelineLayout(vk_device.getDevice(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create pipeline layout");
    }

    vk_device.setObjectName(VK_OBJECT_TYPE_PIPELINE_LAYOUT, (uint64_t)pipelineLayout, "Point Light Pipeline Layout");
}

void Faye::PointLightRenderSystem::createPipeline(VkFormat colorFormat, VkFormat motionFormat, VkFormat depthFormat)
{
    assert(pipelineLayout != nullptr && "Pipeline layout is null");

    PipelineConfigInfo pipelineConfig{};
    VulkanPipeline::defaultPipelineConfigInfo(pipelineConfig);
    pipelineConfig.attributeDescriptions.clear();
    pipelineConfig.bindingDescriptions.clear();
    pipelineConfig.colorBlendAttachments.resize(2, pipelineConfig.colorBlendAttachments.front());
    pipelineConfig.colorAttachmentFormats = {colorFormat, motionFormat};
    pipelineConfig.depthAttachmentFormat = depthFormat;
    pipelineConfig.pipelineLayout = pipelineLayout;

    vk_pipeline = std::make_unique<VulkanPipeline>(
        vk_device,
        Paths::compiledShader("point_light.vert").string(),
        Paths::compiledShader("point_light.frag").string(),
        pipelineConfig);
}

void Faye::PointLightRenderSystem::render(FrameContext &frameContext, const RenderSceneSnapshot &renderScene)
{
    vk_pipeline->bind(frameContext.commandBuffer);

    vkCmdSetCullMode(frameContext.commandBuffer, VK_CULL_MODE_NONE);
    vkCmdSetFrontFace(frameContext.commandBuffer, VK_FRONT_FACE_CLOCKWISE);
    vkCmdSetPrimitiveTopology(frameContext.commandBuffer, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    vkCmdSetDepthTestEnable(frameContext.commandBuffer, VK_TRUE);
    vkCmdSetDepthWriteEnable(frameContext.commandBuffer, VK_TRUE);
    vkCmdSetDepthCompareOp(frameContext.commandBuffer, VK_COMPARE_OP_LESS);

    auto bufferInfo = frameContext.globalBuffer->descriptorInfo();
    VulkanDescriptorWriter(globalDescriptorSetLayout)
        .writeBuffer(0, &bufferInfo)
        .writeImage(1, &frameContext.prepassDepthInfo)
        .pushDescriptors(frameContext.commandBuffer, pipelineLayout, 0);

    for (auto &obj : renderScene.pointLights)
    {
        PointLightPushConstantData push{};
        push.position = glm::vec4(obj.transform->translation, 1.f);
        push.color = glm::vec4(obj.color, obj.intensity);
        push.radius = obj.radius;

        vkCmdPushConstants(
            frameContext.commandBuffer,
            pipelineLayout,
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            0,
            sizeof(PointLightPushConstantData),
            &push);
        vkCmdDraw(frameContext.commandBuffer, 6, 1, 0, 0);
    };
}
