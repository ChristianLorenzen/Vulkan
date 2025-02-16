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

#include "Structures/Vertex.hpp"
#include "Window/Window.hpp"
#include "File.hpp"
#include "Physics/FrameTimer.hpp"
#include "Camera/Camera.hpp"
#include "vk_device.hpp"
#include "vkTypes.hpp"

#include "quill/Logger.h"

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

		void drawFrame(Camera &camera);

		bool framebufferResized = false;

		FrameTimer timer;
		VulkanDevice *vk_device;

		//VmaAllocator allocator;

	private:
		quill::Logger *logger;

		Window *window;

		const std::vector<const char *> validationLayers = {
		    "VK_LAYER_KHRONOS_validation"};

		const std::vector<const char *> deviceExtensions = {
		    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
		    "VK_KHR_portability_subset"};

		uint32_t MAX_FRAMES_IN_FLIGHT = 2;
		uint32_t currentFrame = 0;


		VkBuffer vertexBuffer;
		VkDeviceMemory vertexBufferMemory;
		VkBuffer indexBuffer;
		VkDeviceMemory indexBufferMemory;

		std::vector<VkBuffer> uniformBuffers;
		std::vector<VkDeviceMemory> uniformBuffersMemory;
		std::vector<void *> uniformBuffersMapped;

		// Swapchain
		VkSwapchainKHR swapChain;
		std::vector<VkImage> swapChainImages;
		VkFormat swapChainImageFormat;
		VkExtent2D swapChainExtent;
		std::vector<VkImageView> swapChainImageViews;

		// Create Render Pass
		VkDescriptorSetLayout descriptorSetLayout;
		VkPipelineLayout pipelineLayout;
		VkRenderPass renderPass;
		VkPipeline graphicsPipeline;

		std::vector<VkFramebuffer> swapChainFramebuffers;

		VkDescriptorPool descriptorPool;
		std::vector<VkDescriptorSet> descriptorSets;

		std::vector<VkCommandBuffer> commandBuffers;

		std::vector<VkSemaphore> imageAvailableSemaphores;
		std::vector<VkSemaphore> renderFinishedSemaphores;
		std::vector<VkFence> inFlightFences;

		uint32_t mipLevels;
		VkImage textureImage;
		VkDeviceMemory textureImageMemory;
		VkImageView textureImageView;

		VkSampler textureSampler;

		VkImage depthImage;
		VkDeviceMemory depthImageMemory;
		VkImageView depthImageView;

		std::vector<Vertex> vertices;
		std::vector<uint32_t> indices;

		VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT;

		VkImage colorImage;
		VkDeviceMemory colorImageMemory;
		VkImageView colorImageView;


		void initImGui();

		void createSwapChain();


		VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &availableFormats);
		VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR> &availablePresetModes);
		VkExtent2D channelSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities);
		void createImageViews();

		void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels);

		bool hasStencilComponent(VkFormat format);
		VkFormat findDepthFormat();
		VkFormat findSupportedFormat(const std::vector<VkFormat> &candidates, VkImageTiling tiling, VkFormatFeatureFlags features);

		void generateMipmaps(VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels);

		void createRenderPass();
		void createDescriptorSetLayout();
		void createGraphicsPipeline();
		void createFramebuffers();
		void createCommandPools();
		void createDepthResources();
		void createTextureImage(std::string texturePath);
		void createTextureImageView();
		VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels);
		void createTextureSampler();
		void createImage(uint32_t width, uint32_t height, uint32_t mipLevels, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage &image, VkDeviceMemory &imageMemory);
		void loadModel(std::string modelPath);
		void createVertexBuffer();
		void createIndexBuffer();
		void createUniformBuffers();
		void createDescriptorPool();
		void createDescriptorSets();
		void updateUniformBuffer(uint32_t currentImage, Camera &camera);

		void createCommandBuffers();
		void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
		VkShaderModule createShaderModule(const std::vector<char> &code);
		void createSyncObjects();

		void recreateSwapChain();
		void cleanupSwapChain();

		void cleanup();
	};

} // namespace
