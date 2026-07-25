#define VK_USER_PLATFORM_MACOS_MVK
#include <vulkan/vulkan.h>

#include <cassert>
#include <stdexcept>

#include "editor_grid_render_system.hpp"

#include "VulkanBuffer.hpp"
#include "Core/Path/Paths.hpp"
#include "Core/Logging/Logger.hpp"
#include "quill/LogMacros.h"

using namespace Faye;

namespace
{
	// std140-compatible mirror of the `Push` block in editor_grid.frag. Four
	// vec4s followed by four floats == 80 bytes, comfortably inside the 128-byte
	// push-constant floor guaranteed by every Vulkan implementation.
	struct EditorGridPushConstantData
	{
		glm::vec4 thinLineColor{};
		glm::vec4 thickLineColor{};
		glm::vec4 xAxisColor{};
		glm::vec4 zAxisColor{};
		float cellSize = 1.0f;
		float minPixelsBetweenCells = 2.0f;
		float maxDistance = 250.0f;
		float planeHeight = 0.0f;
	};

	static_assert(sizeof(EditorGridPushConstantData) == 80,
				  "EditorGridPushConstantData layout drift vs editor_grid.frag");
}

Faye::EditorGridRenderSystem::EditorGridRenderSystem(VulkanDevice &device,
													 VkFormat colorFormat,
													 VkFormat motionFormat,
													 VkFormat depthFormat,
													 VulkanDescriptorSetLayout &globalSetLayout)
	: vk_device(device), globalDescriptorSetLayout(globalSetLayout)
{
	LOG_INFO(Logger::get(), "Creating editor grid pipeline...");
	createPipelineLayout(globalSetLayout.getDescriptorSetLayout());
	createPipeline(colorFormat, motionFormat, depthFormat);
}

Faye::EditorGridRenderSystem::~EditorGridRenderSystem()
{
	if (pipelineLayout != VK_NULL_HANDLE)
	{
		vkDestroyPipelineLayout(vk_device.getDevice(), pipelineLayout, nullptr);
	}
}

void Faye::EditorGridRenderSystem::createPipelineLayout(VkDescriptorSetLayout globalSetLayout)
{
	VkPushConstantRange pushConstantRange{};
	// The vertex stage does not read the push block, but declaring a single
	// range across both stages keeps the layout identical to the GLSL
	// declaration and avoids a second range for no benefit.
	pushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	pushConstantRange.offset = 0;
	pushConstantRange.size = sizeof(EditorGridPushConstantData);

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

void Faye::EditorGridRenderSystem::createPipeline(VkFormat colorFormat, VkFormat motionFormat, VkFormat depthFormat)
{
	assert(pipelineLayout != VK_NULL_HANDLE && "Editor grid pipeline layout is null");

	PipelineConfigInfo pipelineConfig{};
	VulkanPipeline::defaultPipelineConfigInfo(pipelineConfig);

	// No vertex buffer: the fullscreen triangle is generated from gl_VertexIndex.
	pipelineConfig.attributeDescriptions.clear();
	pipelineConfig.bindingDescriptions.clear();

	// The scene pass has two colour attachments (scene colour + motion vectors).
	pipelineConfig.colorBlendAttachments.resize(2, pipelineConfig.colorBlendAttachments.front());

	// Attachment 0: standard alpha-over so grid lines composite onto the shaded
	// scene rather than replacing it.
	auto &colorAttachment = pipelineConfig.colorBlendAttachments[0];
	colorAttachment.blendEnable = VK_TRUE;
	colorAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	colorAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	colorAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

	// Attachment 1: the grid is a non-physical editor overlay and must not
	// contribute motion vectors, or TAA/motion-blur consumers downstream would
	// smear it. Masking writes off is cheaper and safer than writing zeroes.
	auto &motionAttachment = pipelineConfig.colorBlendAttachments[1];
	motionAttachment.blendEnable = VK_FALSE;
	motionAttachment.colorWriteMask = 0;

	// (attachmentCount / pAttachments are re-derived from the vector inside
	// VulkanPipeline::createGraphicsPipeline, so no fixup is needed here.)

	pipelineConfig.colorAttachmentFormats = {colorFormat, motionFormat};
	pipelineConfig.depthAttachmentFormat = depthFormat;
	pipelineConfig.pipelineLayout = pipelineLayout;

	vk_pipeline = std::make_unique<VulkanPipeline>(
		vk_device,
		Paths::compiledShader("editor_grid.vert").string(),
		Paths::compiledShader("editor_grid.frag").string(),
		pipelineConfig);
}

void Faye::EditorGridRenderSystem::render(FrameContext &frameContext, const EditorGridSettings &settings)
{
	if (!settings.enabled)
	{
		return;
	}

	vk_pipeline->bind(frameContext.commandBuffer);

	vkCmdSetCullMode(frameContext.commandBuffer, VK_CULL_MODE_NONE);
	vkCmdSetFrontFace(frameContext.commandBuffer, VK_FRONT_FACE_CLOCKWISE);
	vkCmdSetPrimitiveTopology(frameContext.commandBuffer, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

	// Test against scene depth so geometry occludes the grid, but do not write:
	// the grid is a transparent overlay and must not block anything drawn after
	// it, nor perturb depth-consuming post-process effects (fog, DOF).
	vkCmdSetDepthTestEnable(frameContext.commandBuffer, VK_TRUE);
	vkCmdSetDepthWriteEnable(frameContext.commandBuffer, VK_FALSE);
	vkCmdSetDepthCompareOp(frameContext.commandBuffer, VK_COMPARE_OP_LESS);

	// Set 0 carries the camera matrices the grid needs: inverseProjection and
	// inverseView to build the eye ray, projection/view to reproject for depth.
	auto bufferInfo = frameContext.globalBuffer->descriptorInfo();
	VulkanDescriptorWriter(globalDescriptorSetLayout)
		.writeBuffer(0, &bufferInfo)
		.writeImage(1, &frameContext.prepassDepthInfo)
		.pushDescriptors(frameContext.commandBuffer, pipelineLayout, 0);

	EditorGridPushConstantData push{};
	push.thinLineColor = settings.thinLineColor;
	push.thickLineColor = settings.thickLineColor;
	push.xAxisColor = settings.xAxisColor;
	push.zAxisColor = settings.zAxisColor;
	push.cellSize = settings.cellSize;
	push.minPixelsBetweenCells = settings.minPixelsBetweenCells;
	push.maxDistance = settings.maxDistance;
	push.planeHeight = settings.planeHeight;

	vkCmdPushConstants(frameContext.commandBuffer,
					   pipelineLayout,
					   VK_SHADER_STAGE_FRAGMENT_BIT,
					   0,
					   sizeof(EditorGridPushConstantData),
					   &push);

	vkCmdDraw(frameContext.commandBuffer, 3, 1, 0, 0);
}
