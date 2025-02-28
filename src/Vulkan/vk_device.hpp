#pragma once

#include <vulkan/vulkan.h>
#include <vector>

#include "vk_types.hpp"
#include "../Window/Window.hpp"

#include "quill/LogMacros.h"

#include "Logging/Logger.hpp"

namespace Faye {
    class VulkanDevice {
        public:
            VulkanDevice(Window &window);
            ~VulkanDevice();

            VulkanDevice(const VulkanDevice &) = delete;
            VulkanDevice &operator=(const VulkanDevice &) = delete;


            VkInstance getInstance() { return instance; }
            VkCommandPool* getCommandPool() { return &commandPool; }
            VkDevice getDevice() { return device; }
            VkPhysicalDevice getPhysicalDevice() { return physicalDevice; }
            VkSurfaceKHR getSurface() { return surface; }
            VkQueue getGraphicsQueue() { return graphicsQueue; }
            VkQueue getPresentQueue() { return presentQueue; }

            SwapChainSupportDetails getSwapchainSupport() { return querySwapChainSupport(physicalDevice); }
            QueueFamilyIndices findPhysicalQueueFamilies() { return findQueueFamilies(physicalDevice); }    
            VkFormat findSupportedFormat(const std::vector<VkFormat> &candidates, VkImageTiling tiling, VkFormatFeatureFlags features);

            uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

            VkPhysicalDeviceProperties properties;

            // TODO should be private
            void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer &buffer, VkDeviceMemory &bufferMemory);
            void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
            void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);
            
            VkCommandBuffer beginSingleTimeCommands();
            void endSingleTimeCommands(VkCommandBuffer commandBuffer);

            SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);

            QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);

            void createImageWithInfo(const VkImageCreateInfo &imageInfo, VkMemoryPropertyFlags properties, VkImage &image, VkDeviceMemory &imageMemory);


        private:
            void createInstance();
            void createSurface();
            void createPhysicalDevice();
            void createLogicalDevice();
            void createCommandPools();

            bool isDeviceSuitable(VkPhysicalDevice device);
            std::vector<const char *> getRequiredExtensions();
            bool checkValidationLayerSupport();
            bool checkDeviceExtensionSupport(VkPhysicalDevice device);
            VkSampleCountFlagBits getMaxUsableSampleCount();
        
            VkInstance instance;
            VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;

            Window &window;
            VkCommandPool commandPool;

            VkDevice device;
            VkSurfaceKHR surface;
            VkQueue graphicsQueue;
            VkQueue presentQueue;

            const std::vector<const char *> validationLayers = {
                "VK_LAYER_KHRONOS_validation"};

            const std::vector<const char *> deviceExtensions = {
                VK_KHR_SWAPCHAIN_EXTENSION_NAME,
                "VK_KHR_portability_subset"};
    };
}