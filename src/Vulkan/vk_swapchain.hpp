#pragma once

#include <vulkan/vulkan.h>
#include <vector>


#include "Swapchain.hpp"

#include "vk_device.hpp"
#include "vk_swapchain.hpp"
#include "vk_types.hpp"

namespace Faye {
    class VulkanSwapchain : public Swapchain {
    public:
        static constexpr int MAX_FRAMES_IN_FLIGHT = 2;

        VulkanSwapchain(VulkanDevice* device, VkExtent2D extent);
        ~VulkanSwapchain();

        VulkanSwapchain(const VulkanSwapchain &) = delete;
        void operator=(const VulkanSwapchain &) = delete;

        VkFramebuffer getFrameBuffer(uint32_t index) const { return swapChainFramebuffers[index]; }
        VkRenderPass getRenderPass() const { return renderPass; }
        VkImageView getImageView(uint32_t index) const { return swapChainImageViews[index]; }
        size_t imageCount() const { return swapChainImages.size(); }
        VkFormat getSwapChainImageFormat() const { return swapChainImageFormat; }
        VkExtent2D getSwapChainExtent() const { return swapChainExtent; }
        uint32_t width() { return swapChainExtent.width; }
        uint32_t height() { return swapChainExtent.height; }

        float extentAspectRatio() { return static_cast<float>(swapChainExtent.width) / static_cast<float>(swapChainExtent.height); }

        VkResult acquireNextImage(uint32_t* imageIndex);
        VkResult submitCommandBuffers(const VkCommandBuffer* buffers, uint32_t* imageIndex);

    private:
        void createSwapChain();
        void recreateSwapChain();
        void createDepthResources();
        void createRenderPass();
        void createFramebuffers();
        void createSyncObjects();
                
        void createImageViews();
        
        void createImage(uint32_t width, uint32_t height, uint32_t mipLevels, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage &image, VkDeviceMemory &imageMemory);
        VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels);

        VkFormat findDepthFormat();

        VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &availableFormats);
        VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR> &availablePresetModes);
        VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities);
        
        bool framebufferResized = false;

        VkFormat swapChainImageFormat;
        VkExtent2D swapChainExtent;

        std::vector<VkFramebuffer> swapChainFramebuffers;
        VkRenderPass renderPass;

        std::vector<VkImage> depthImages;
        std::vector<VkDeviceMemory> depthImageMemorys;
        std::vector<VkImageView> depthImageViews;
        std::vector<VkImage> swapChainImages;
        std::vector<VkImageView> swapChainImageViews;

        VulkanDevice* device;
        VkExtent2D windowExtent;

        VkSwapchainKHR swapChain;

        std::vector<VkSemaphore> imageAvailableSemaphores;
        std::vector<VkSemaphore> renderFinishedSemaphores;
        std::vector<VkFence> inFlightFences;
        std::vector<VkFence> imagesInFlight;
        size_t currentFrame = 0;
    };
}