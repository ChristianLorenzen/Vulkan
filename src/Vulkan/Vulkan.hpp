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
#include "Physics/FrameTimer.hpp"
#include "Camera/Camera.hpp"
#include "vk_device.hpp"
#include "vk_pipeline.hpp"
#include "vk_renderer.hpp"
#include "vk_types.hpp"
#include "vk_descriptors.hpp"


#include "Logging/Logger.hpp"

#include "Structures/GameObject.hpp"


namespace Faye
{

	class Vulkan
	{
	public:
		Vulkan(Window *win);
		~Vulkan();

		Vulkan(const Vulkan &) = delete;
		void operator=(const Vulkan &) = delete;
		Vulkan(Vulkan &&) = delete;
		Vulkan &operator=(Vulkan &&) = delete;

		void run();

		FrameTimer timer;

		VulkanDevice *getVkDevice() { return vk_device; }

	private:
		// TODO : Temp for vk_pipeline setup.
		const uint32_t WIDTH = 1300;
		const uint32_t HEIGHT = 900;

		void loadGameObjects();
		
		Window *window;
		VulkanDevice *vk_device;
		VulkanRenderer *vk_renderer;

		std::unique_ptr<VulkanDescriptorPool> globalPool{};
		// Descriptor pool passed to the ImGui initialization function.
		std::unique_ptr<VulkanDescriptorPool> imGUIPool{};
		std::vector<GameObject> gameObjects;

	};

} // namespace
