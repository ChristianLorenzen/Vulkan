#define VK_USER_PLATFORM_MACOS_MVK
#include <vulkan/vulkan.h>
#include <stdio.h>
#include <unordered_map>

#include "Vulkan.hpp"
#include "point_light_render_system.hpp"

#include "quill/LogMacros.h"

#include <chrono>

using namespace Faye;

struct PointLightPushConstantData
{
    glm::vec4 position{};
    glm::vec4 color{};
    float radius;
};

Faye::PointLightRenderSystem::PointLightRenderSystem(VulkanDevice &device, VkFormat colorFormat, VkFormat motionFormat, VkFormat depthFormat, VkDescriptorSetLayout globalSetLayout) : vk_device(device)
{
    LOG_INFO(Logger::getInstance(), "Creating Vulkan Pipeline Layout...");
    createPipelineLayout(globalSetLayout);
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

    LOG_INFO(Logger::getInstance(), "Created pipelinelayoutinfo struct...");

    if (vkCreatePipelineLayout(vk_device.getDevice(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create pipeline layout");
    }
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
        "./src/shaders/compiled/point_light.vert.spv",
        "./src/shaders/compiled/point_light.frag.spv",
        pipelineConfig);
}

void Faye::PointLightRenderSystem::update(FrameContext &frameContext, const RenderSceneSnapshot &renderScene, GlobalUBO &ubo)
{
    int lightIndex = 0;
    for (auto &obj : renderScene.pointLights)
    {
        ubo.pointLights[lightIndex].position = glm::vec4(obj.transform->translation, 1.f);
        ubo.pointLights[lightIndex].color = glm::vec4(obj.color, obj.intensity);
        lightIndex++;
    }
    ubo.numLights = lightIndex;
}

void Faye::PointLightRenderSystem::render(FrameContext &frameContext, const RenderSceneSnapshot &renderScene)
{
    vk_pipeline->bind(frameContext.commandBuffer);

    vkCmdBindDescriptorSets(
        frameContext.commandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipelineLayout,
        0,
        1,
        &frameContext.globalDescriptorSet,
        0,
        nullptr);

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
