#pragma once

#define GLFW_INCLUDE_VULKAN
#define VK_USER_PLATFORM_MACOS_MVK
#include <vulkan/vulkan.h>
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

#include "quill/Logger.h"

namespace Faye {

    struct QueueFamilyIndices {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;

        bool isComplete() {
            return graphicsFamily.has_value() && presentFamily.has_value();
        }
    };

    struct SwapChainSupportDetails {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

    class Vulkan {
        public:
            Vulkan(Window* win);
            ~Vulkan();

            Vulkan(const Vulkan &) = delete;
            void operator=(const Vulkan &) = delete;
            Vulkan(Vulkan &&) = delete;
            Vulkan &operator=(Vulkan &&) = delete;

            VkInstance getInstance() { return instance; }
            VkPhysicalDevice getPhysicalDevice() { return physicalDevice; }
            VkSurfaceKHR* getSurface() { return &surface; }
            VkDevice getDevice() { return device; }
            VkQueue getGraphicsQueue() { return graphicsQueue; }
            VkQueue getPresentQueue() { return presentQueue; }

            void drawFrame();

            bool framebufferResized = false;

        private:
            quill::Logger* logger;

            Window* window;

            const std::vector<Vertex> vertices = {
                {{-0.5f, -0.5f}, {1.0f, 1.0f, 1.0f}},
                {{0.5f, -0.5f}, {0.0f, 0.0f, 0.0f}},
                {{0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
                {{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}}
            };


            const std::vector<const char*> validationLayers = {
                "VK_LAYER_KHRONOS_validation"
            };

            const std::vector<const char*> deviceExtensions = {
                VK_KHR_SWAPCHAIN_EXTENSION_NAME,
                "VK_KHR_portability_subset"
            };

            VkInstance instance = nullptr;

            VkSurfaceKHR surface = nullptr;
            VkPhysicalDevice physicalDevice = nullptr;
            VkDevice device = nullptr;

            VkBuffer vertexBuffer;
            VkDeviceMemory vertexBufferMemory;

            // Queues
            VkQueue graphicsQueue = nullptr;
            VkQueue presentQueue = nullptr;

            // Swapchain
            VkSwapchainKHR swapChain;
            std::vector<VkImage> swapChainImages;
            VkFormat swapChainImageFormat;
            VkExtent2D swapChainExtent;
            std::vector<VkImageView> swapChainImageViews;

            //Create Render Pass
            VkPipelineLayout pipelineLayout;
            VkRenderPass renderPass;
            VkPipeline graphicsPipeline;

            std::vector<VkFramebuffer> swapChainFramebuffers;

            VkCommandPool commandPool;
            VkCommandBuffer commandBuffer;

            VkSemaphore imageAvailableSemaphore;
            VkSemaphore renderFinishedSemaphore;
            VkFence inFlightFence;

            void onInit();
            void createSurface();
            void createPhysicalDevice();
            void createLogicalDevice();
            void createSwapChain();
            bool checkValidationLayerSupport();
            bool isDeviceSuitable(VkPhysicalDevice device);
            bool checkDeviceExtensionSupport(VkPhysicalDevice device);
            QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
            SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);

            uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

            VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
            VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresetModes);
            VkExtent2D channelSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
            void createImageViews();

            void createRenderPass() ;
            void createGraphicsPipeline();
            void createFramebuffers();
            void createCommandPools();
            void createVertexBuffer();
            void createCommandBuffer();
            void recordCommandBuffer(uint32_t imageIndex);
            VkShaderModule createShaderModule(const std::vector<char>& code);
            void createSyncObjects();

            void recreateSwapChain();
            void cleanupSwapChain();

            void cleanup();
    };

} // namespace
