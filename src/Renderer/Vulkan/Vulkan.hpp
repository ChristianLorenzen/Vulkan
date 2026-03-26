#pragma once

#define GLFW_INCLUDE_VULKAN
#define VK_USER_PLATFORM_MACOS_MVK
#include <vulkan/vulkan.h>

// #define VMA_IMPLEMENTATION
// #include <vk_mem_alloc.h>

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
#include "VulkanBuffer.hpp"
#include "vk_render_system.hpp"
#include "vk_device.hpp"
#include "vk_pipeline.hpp"
#include "vk_renderer.hpp"
#include "vk_types.hpp"
#include "vk_descriptors.hpp"

#include "Core/Logging/Logger.hpp"

#include "Scene/Entities/GameObject.hpp"

namespace Faye
{
	struct VulkanFrameInput
	{
		Camera &camera;
		int frameTimeMs = 0;
		int averageFps = 0;
	};

	class Vulkan
	{
	public:
		explicit Vulkan(Window &win);
		~Vulkan();

		Vulkan(const Vulkan &) = delete;
		void operator=(const Vulkan &) = delete;
		Vulkan(Vulkan &&) = delete;
		Vulkan &operator=(Vulkan &&) = delete;

		void renderFrame(const VulkanFrameInput &frameInput);
		float getAspectRatio() const;

		VulkanDevice *getVkDevice() { return vk_device.get(); }

	private:
		// TODO : Temp for vk_pipeline setup.
		const uint32_t WIDTH = 1300;
		const uint32_t HEIGHT = 900;

		void loadGameObjects();

		Window &window;
		std::unique_ptr<VulkanDevice> vk_device;
		std::unique_ptr<VulkanRenderer> vk_renderer;
		std::unique_ptr<VulkanDescriptorSetLayout> globalSetLayout{};
		std::unique_ptr<SimpleRenderSystem> simpleRenderSystem{};

		std::unique_ptr<VulkanDescriptorPool> globalPool{};
		// Descriptor pool passed to the ImGui initialization function.
		std::unique_ptr<VulkanDescriptorPool> imGUIPool{};
		std::vector<std::unique_ptr<VulkanBuffer>> uboBuffers;
		std::vector<VkDescriptorSet> globalDescriptorSets;
		std::vector<GameObject> gameObjects;

		void initializeFrameResources();
	};

} // namespace
