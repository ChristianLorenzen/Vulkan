#include <vulkan/vulkan.hpp>

#include <cassert>
#include <stdexcept>

#include "engine/Scene/Scene.hpp"
#include "glm/glm.hpp"

#include "skybox_render_system.hpp"

#include "VulkanBuffer.hpp"
#include "Core/Path/Paths.hpp"


namespace Faye {

    struct SkyboxPushConstantData {
        float rotation = 0.0f;
        float intensity = 1.0f;
    };


    SkyboxRenderSystem::SkyboxRenderSystem(VulkanDevice &device,
										   VkFormat colorFormat,
										   VkFormat motionFormat,
										   VkFormat depthFormat,
										   VulkanDescriptorSetLayout &globalSetLayout)
		: vk_device(device),
		  globalDescriptorSetLayout(globalSetLayout) {
		createPipelineLayout(globalSetLayout.getDescriptorSetLayout());
		createPipeline(colorFormat, motionFormat, depthFormat);
    }
	
    
    SkyboxRenderSystem::~SkyboxRenderSystem() {
		if (pipelineLayout != VK_NULL_HANDLE) vkDestroyPipelineLayout(vk_device.getDevice(), pipelineLayout, nullptr);
	}

    void SkyboxRenderSystem::createPipelineLayout(VkDescriptorSetLayout globalSetLayout)
    {
        VkPushConstantRange pushConstantRange{};

        pushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        pushConstantRange.offset = 0;
        pushConstantRange.size = sizeof(SkyboxPushConstantData);

        VkDescriptorSetLayout setLayouts[] = {globalSetLayout};

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = setLayouts;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

        if (vkCreatePipelineLayout(vk_device.getDevice(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create editor grid pipeline layout");
        }
    }

    void SkyboxRenderSystem::createPipeline(VkFormat colorFormat, VkFormat motionFormat, VkFormat depthFormat)
    {
        assert(pipelineLayout != VK_NULL_HANDLE && "Skybox pipeline layout is null");

        PipelineConfigInfo pipelineConfig{};
        VulkanPipeline::defaultPipelineConfigInfo(pipelineConfig);

        // No vertex buffer: the fullscreen triangle is generated from gl_VertexIndex.
        pipelineConfig.attributeDescriptions.clear();
        pipelineConfig.bindingDescriptions.clear();

        // The scene pass has two colour attachments (scene colour + motion vectors).
        pipelineConfig.colorBlendAttachments.resize(2, pipelineConfig.colorBlendAttachments.front());

        auto &colorAttachment = pipelineConfig.colorBlendAttachments[0];
        colorAttachment.blendEnable = VK_FALSE;
        colorAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        colorAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        colorAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorAttachment.alphaBlendOp = VK_BLEND_OP_ADD;


        auto &motionAttachment = pipelineConfig.colorBlendAttachments[1];
        motionAttachment.blendEnable = VK_FALSE;
        motionAttachment.colorWriteMask = 0;

        pipelineConfig.colorAttachmentFormats = {colorFormat, motionFormat};
        pipelineConfig.depthAttachmentFormat = depthFormat;
        pipelineConfig.pipelineLayout = pipelineLayout;

        vk_pipeline = std::make_unique<VulkanPipeline>(
            vk_device,
            Paths::compiledShader("skybox.vert").string(),
            Paths::compiledShader("skybox.frag").string(),
            pipelineConfig);
    }

    void SkyboxRenderSystem::render(FrameContext &frameContext, const SkyboxSettings &settings)
    {
        vk_pipeline->bind(frameContext.commandBuffer);

        vkCmdSetCullMode(frameContext.commandBuffer, VK_CULL_MODE_NONE);
        vkCmdSetFrontFace(frameContext.commandBuffer, VK_FRONT_FACE_CLOCKWISE);
        vkCmdSetPrimitiveTopology(frameContext.commandBuffer, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

        vkCmdSetDepthTestEnable(frameContext.commandBuffer, VK_TRUE);
        vkCmdSetDepthWriteEnable(frameContext.commandBuffer, VK_FALSE);
        vkCmdSetDepthCompareOp(frameContext.commandBuffer, VK_COMPARE_OP_LESS_OR_EQUAL);


        auto bufferInfo = frameContext.globalBuffer->descriptorInfo();
        VulkanDescriptorWriter(globalDescriptorSetLayout)
            .writeBuffer(0, &bufferInfo)
            .writeImage(1, &frameContext.prepassDepthInfo)
            .writeImage(3, &frameContext.skyCubeInfo)
            .pushDescriptors(frameContext.commandBuffer, pipelineLayout, 0);

        SkyboxPushConstantData push{};
        push.rotation = settings.rotation; // Rotate the skybox slowly over time.
        push.intensity = settings.intensity;

        vkCmdPushConstants(frameContext.commandBuffer,
                        pipelineLayout,
                        VK_SHADER_STAGE_FRAGMENT_BIT,
                        0,
                        sizeof(SkyboxPushConstantData),
                        &push);

        vkCmdDraw(frameContext.commandBuffer, 3, 1, 0, 0);
    }

};