#pragma once

#define GLFW_INCLUDE_VULKAN
#define VK_USER_PLATFORM_MACOS_MVK
#include <vulkan/vulkan.h>

#include <memory>

#include "Renderer/Frame/FrameContext.hpp"
#include "Renderer/View/RenderView.hpp"
#include "vk_descriptors.hpp"
#include "vk_device.hpp"
#include "vk_pipeline.hpp"

namespace Faye
{
	// Infinite ground-plane reference grid for the editor viewport.
	//
	// Deliberately NOT a scene object: it owns no entity, appears in no render
	// snapshot, and is never extracted. Vulkan::renderScene invokes it only when
	// the incoming RenderView asks for it, which in practice means only the
	// editor's view. A runtime shell that builds its own RenderView leaves
	// EditorGridSettings::enabled false and this system is never touched.
	//
	// The whole grid is one full-screen triangle with no vertex buffer; see
	// shaders/editor_grid.vert for why that is enough to cover an infinite plane.
	class EditorGridRenderSystem
	{
	public:
		EditorGridRenderSystem(VulkanDevice &device,
							   VkFormat colorFormat,
							   VkFormat motionFormat,
							   VkFormat depthFormat,
							   VulkanDescriptorSetLayout &globalSetLayout);
		~EditorGridRenderSystem();

		EditorGridRenderSystem(const EditorGridRenderSystem &) = delete;
		void operator=(const EditorGridRenderSystem &) = delete;
		EditorGridRenderSystem(EditorGridRenderSystem &&) = delete;
		EditorGridRenderSystem &operator=(EditorGridRenderSystem &&) = delete;

		// Record the grid draw. Must be called inside the scene render pass,
		// after opaque geometry, so the depth buffer it tests against is
		// complete and its alpha blend composites over the shaded scene.
		void render(FrameContext &frameContext, const EditorGridSettings &settings);

	private:
		void createPipelineLayout(VkDescriptorSetLayout globalSetLayout);
		void createPipeline(VkFormat colorFormat, VkFormat motionFormat, VkFormat depthFormat);

		VulkanDevice &vk_device;
		VulkanDescriptorSetLayout &globalDescriptorSetLayout;
		std::unique_ptr<VulkanPipeline> vk_pipeline;
		VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};
	};
} // namespace Faye
