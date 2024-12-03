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

            // Pre index buffer rectangle
            // const std::vector<Vertex> vertices = {
            //     {{-0.5f, -0.5f}, {1.0f, 1.0f, 1.0f}},
            //     {{0.5f, -0.5f}, {0.0f, 0.0f, 0.0f}},
            //     {{0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
            //     {{0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
            //     {{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}},
            //     {{-0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}}
            // };

            const std::vector<Vertex> vertices = {
                {{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
                {{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}},
                {{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f}},
                {{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}, {1.0f, 1.0f}}
            };

            const std::vector<uint16_t> indices = {
                0, 1, 2, 2, 3, 0
            };

            const std::vector<const char*> validationLayers = {
                "VK_LAYER_KHRONOS_validation"
            };

            const std::vector<const char*> deviceExtensions = {
                VK_KHR_SWAPCHAIN_EXTENSION_NAME,
                "VK_KHR_portability_subset"
            };

            uint32_t MAX_FRAMES_IN_FLIGHT = 2;
            uint32_t currentFrame = 0;

            VkInstance instance = nullptr;

            VkSurfaceKHR surface = nullptr;
            VkPhysicalDevice physicalDevice = nullptr;
            VkDevice device = nullptr;

            VkBuffer vertexBuffer;
            VkDeviceMemory vertexBufferMemory;
            VkBuffer indexBuffer;
            VkDeviceMemory indexBufferMemory;

            std::vector<VkBuffer> uniformBuffers;
            std::vector<VkDeviceMemory> uniformBuffersMemory;
            std::vector<void*> uniformBuffersMapped;

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
            VkDescriptorSetLayout descriptorSetLayout;
            VkPipelineLayout pipelineLayout;
            VkRenderPass renderPass;
            VkPipeline graphicsPipeline;

            std::vector<VkFramebuffer> swapChainFramebuffers;

            VkDescriptorPool descriptorPool;
            std::vector<VkDescriptorSet> descriptorSets;
            VkCommandPool commandPool;

            std::vector<VkCommandBuffer> commandBuffers;

            std::vector<VkSemaphore> imageAvailableSemaphores;
            std::vector<VkSemaphore> renderFinishedSemaphores;
            std::vector<VkFence> inFlightFences;

            VkImage textureImage;
            VkDeviceMemory textureImageMemory;
            VkImageView textureImageView;

            VkSampler textureSampler;

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

            VkCommandBuffer beginSingleTimeCommands();
            void endSingleTimeCommands(VkCommandBuffer commandBuffer);

            void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
            void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);

            void createRenderPass() ;
            void createDescriptorSetLayout();
            void createGraphicsPipeline();
            void createFramebuffers();
            void createCommandPools();
            void createTextureImage();
            void createTextureImageView();
            VkImageView createImageView(VkImage image, VkFormat format);
            void createTextureSampler();
            void createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory);
            void createVertexBuffer();
            void createIndexBuffer();
            void createUniformBuffers();
            void createDescriptorPool();
            void createDescriptorSets();
            void updateUniformBuffer(uint32_t currentImage);
            void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
            void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
            
            void createCommandBuffers();
            void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
            VkShaderModule createShaderModule(const std::vector<char>& code);
            void createSyncObjects();

            void recreateSwapChain();
            void cleanupSwapChain();

            void cleanup();
    };

} // namespace
