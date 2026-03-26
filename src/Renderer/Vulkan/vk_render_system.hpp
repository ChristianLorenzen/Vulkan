#pragma once

#define GLFW_INCLUDE_VULKAN
#define VK_USER_PLATFORM_MACOS_MVK
#include <vulkan/vulkan.h>

//#define VMA_IMPLEMENTATION
//#include <vk_mem_alloc.h>

#include <stdio.h>
#include <stdlib.h>
#include <exception>
#include <iostream>
#include <optional>
#include <set>
#include <fstream>
#include <memory>

#include "Platform/Window/Window.hpp"
#include "Renderer/Frame/FrameContext.hpp"
#include "Renderer/Resources/Model.hpp"
#include "Renderer/Scene/RenderScene.hpp"
#include "Renderer/Resources/Vertex.hpp"
#include "vk_device.hpp"
#include "vk_pipeline.hpp"
#include "vk_types.hpp"

#include "Core/Logging/Logger.hpp"


namespace Faye
{

	class SimpleRenderSystem
	{
	public:
		SimpleRenderSystem(VulkanDevice &device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout);
		~SimpleRenderSystem();

		SimpleRenderSystem(const SimpleRenderSystem &) = delete;
		void operator=(const SimpleRenderSystem &) = delete;
		SimpleRenderSystem(SimpleRenderSystem &&) = delete;
		SimpleRenderSystem &operator=(SimpleRenderSystem &&) = delete;

		void renderScene(FrameContext &frameContext, const RenderSceneSnapshot &renderScene);

	private:

		VulkanDevice& vk_device;
		std::unique_ptr<VulkanPipeline> vk_pipeline;

		VkPipelineLayout pipelineLayout;
        
		void createPipelineLayout(VkDescriptorSetLayout globalSetLayout);
		void createPipeline(VkRenderPass renderPass);
	};

} // namespace
