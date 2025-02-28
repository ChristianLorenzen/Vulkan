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

#include "Structures/Vertex.hpp"
#include "Structures/Model.hpp"
#include "Window/Window.hpp"
#include "File.hpp"
#include "vk_device.hpp"
#include "vk_pipeline.hpp"
#include "vk_types.hpp"
#include "Camera/Camera.hpp"
#include "Structures/FrameInfo.hpp"

#include "Logging/Logger.hpp"

#include "Structures/GameObject.hpp"


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

        void renderGameObjects(FrameInfo &frameInfo, std::vector<GameObject> &gameObjects);

	private:

		VulkanDevice& vk_device;
		std::unique_ptr<VulkanPipeline> vk_pipeline;

		VkPipelineLayout pipelineLayout;
        
		void createPipelineLayout(VkDescriptorSetLayout globalSetLayout);
		void createPipeline(VkRenderPass renderPass);
	};

} // namespace
