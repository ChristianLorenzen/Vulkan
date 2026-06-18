#pragma once

#include <vulkan/vulkan.h>
#include <vector>

#include "vk_defines.hpp"
#include "vk_types.hpp"
#include "vk_mem_alloc.h"
#include "Platform/Window/Window.hpp"

#include "quill/LogMacros.h"

#include "Core/Logging/Logger.hpp"

namespace Faye
{
    class VulkanDevice
    {
    public:
        VulkanDevice(Window &window);
        ~VulkanDevice();

        VulkanDevice(const VulkanDevice &) = delete;
        VulkanDevice &operator=(const VulkanDevice &) = delete;

        VkInstance getInstance() { return instance; }
        VkCommandPool *getCommandPool() { return &commandPool; }
        VkDevice getDevice() { return device; }
        VkPhysicalDevice getPhysicalDevice() { return physicalDevice; }
        VkSurfaceKHR getSurface() { return surface; }
        VkQueue getGraphicsQueue() { return graphicsQueue; }
        VkQueue getPresentQueue() { return presentQueue; }
        uint32_t getGraphicsQueueFamilyIndex() const { return graphicsQueueFamilyIndex; }
        VmaAllocator getAllocator() { return allocator; }
        VkPipelineCache getPipelineCache() { return pipelineCache; }

        SwapChainSupportDetails getSwapchainSupport() { return querySwapChainSupport(physicalDevice); }
        QueueFamilyIndices findPhysicalQueueFamilies() { return cachedQueueFamilies; }
        VkFormat findSupportedFormat(const std::vector<VkFormat> &candidates, VkImageTiling tiling, VkFormatFeatureFlags features);

        uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

        VkPhysicalDeviceProperties properties;

        // TODO should be private
        void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, const VmaAllocationCreateInfo &allocInfo, VkBuffer &buffer, VmaAllocation &allocation);
        void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
        void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);

        VkCommandBuffer beginSingleTimeCommands();
        void endSingleTimeCommands(VkCommandBuffer commandBuffer);

        SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);

        QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);

        void createImageWithInfo(const VkImageCreateInfo &imageInfo, const VmaAllocationCreateInfo &allocInfo, VkImage &image, VmaAllocation &allocation);

    private:
        void createInstance();
        void createSurface();
        void createPhysicalDevice();
        void createLogicalDevice();
        void createCommandPools();
        void setupDebugMessenger();
        void destroyDebugMessenger();
        void createAllocator();
        void createPipelineCache();
        void savePipelineCache();
        void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT &createInfo) const;

        bool isDeviceSuitable(VkPhysicalDevice device);
        bool shouldEnableValidationLayers();
        std::vector<const char *> getRequiredExtensions();
        bool checkValidationLayerSupport();
        bool checkDeviceExtensionSupport(VkPhysicalDevice device);
        VkSampleCountFlagBits getMaxUsableSampleCount();

        VmaAllocator allocator = VK_NULL_HANDLE;
        VkPipelineCache pipelineCache = VK_NULL_HANDLE;

        VkInstance instance;
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;

        Window &window;
        VkCommandPool commandPool;

        VkDevice device;
        VkSurfaceKHR surface;
        VkQueue graphicsQueue;
        VkQueue presentQueue;
        VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
        uint32_t graphicsQueueFamilyIndex = 0;
        QueueFamilyIndices cachedQueueFamilies{};
        bool validationLayersEnabled = false;

        const std::vector<const char *> validationLayers = {
            "VK_LAYER_KHRONOS_validation"};

#ifdef __APPLE__
        const std::vector<const char *> deviceExtensions = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
            "VK_KHR_portability_subset"};
#else
        const std::vector<const char *> deviceExtensions = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME};
#endif
    };
}