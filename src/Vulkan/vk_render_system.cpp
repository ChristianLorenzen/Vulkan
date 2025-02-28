#define VK_USER_PLATFORM_MACOS_MVK
#include <vulkan/vulkan.h>
#include <stdio.h>
#include <unordered_map>


#include "Vulkan.hpp"
#include "Window/Window.hpp"
#include "Structures/Model.hpp"
#include "Structures/Vertex.hpp"
#include "Structures/UBO.hpp"
#include "vk_render_system.hpp"

#include "quill/LogMacros.h"


#include <chrono>


using namespace Faye;


struct SimplePushConstantData {
    glm::mat4 modelMatrix{1.0f};
    glm::mat4 normalMatrix{};
};


Faye::SimpleRenderSystem::SimpleRenderSystem(VulkanDevice &device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout) : vk_device(device)
{
    LOG_INFO(Logger::getInstance(), "Creating Vulkan Pipeline Layout...");
    createPipelineLayout(globalSetLayout);
    createPipeline(renderPass);

}

Faye::SimpleRenderSystem::~SimpleRenderSystem()
{
    vkDestroyPipelineLayout(vk_device.getDevice(), pipelineLayout, nullptr);
}



// ------------------------------------- Conversion Functions ------------------------------------- //
void Faye::SimpleRenderSystem::createPipelineLayout(VkDescriptorSetLayout globalSetLayout) {
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(SimplePushConstantData);

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

void Faye::SimpleRenderSystem::createPipeline(VkRenderPass renderPass) {
    assert(pipelineLayout != nullptr && "Pipeline layout is null");

    PipelineConfigInfo pipelineConfig {};
    VulkanPipeline::defaultPipelineConfigInfo(pipelineConfig);

    pipelineConfig.renderPass = renderPass;
    pipelineConfig.pipelineLayout = pipelineLayout;

    vk_pipeline = std::make_unique<VulkanPipeline>(
        vk_device,
        "./src/shaders/vert.spv",
        "./src/shaders/frag.spv",
        pipelineConfig);
}


void Faye::SimpleRenderSystem::renderGameObjects(FrameInfo &frameInfo, std::vector<GameObject> &gameObjects) {
    vk_pipeline->bind(frameInfo.commandBuffer);

    vkCmdBindDescriptorSets(
        frameInfo.commandBuffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipelineLayout,
        0,
        1,
        &frameInfo.globalDescriptorSet,
        0,
        nullptr
    );

    for (auto& go : gameObjects) {
        SimplePushConstantData push{};
        push.modelMatrix = go.transform.mat4();
        push.normalMatrix = go.transform.normalMatrix();

        vkCmdPushConstants(
            frameInfo.commandBuffer,
            pipelineLayout,
            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
            0,
            sizeof(SimplePushConstantData),
            &push
        );

        go.model->bind(frameInfo.commandBuffer);
        go.model->draw(frameInfo.commandBuffer);
    }
}
