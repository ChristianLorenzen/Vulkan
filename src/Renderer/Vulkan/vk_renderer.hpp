#pragma once

#include "Platform/Window/Window.hpp"
#include "vk_device.hpp"
#include "vk_swapchain.hpp"

#include <memory>
#include <vector>

#include <cassert>

namespace Faye
{
    class VulkanRenderer
    {

    public:
        VulkanRenderer(Window &window, VulkanDevice &device);
        ~VulkanRenderer();

        VulkanRenderer(const VulkanRenderer &) = delete;
        VulkanRenderer &operator=(const VulkanRenderer &) = delete;

        VkCommandBuffer beginFrame();
        void endFrame();

        void beginSceneRenderPass(VkCommandBuffer commandBuffer);
        void endSceneRenderPass(VkCommandBuffer commandBuffer);
        void beginSwapchainRenderPass(VkCommandBuffer commandBuffer);
        void endSwapchainRenderPass(VkCommandBuffer commandBuffer);

        VkRenderPass getSceneRenderPass() const { return sceneRenderPass; }
        VkRenderPass getSwapChainRenderPass() const { return vk_swapchain->getRenderPass(); }
        float getAspectRatio() const { return vk_swapchain->extentAspectRatio(); }
        VkDescriptorSet getSceneViewportDescriptorSet() const { return sceneViewportDescriptorSets[currentFrameIndex]; }
        VkExtent2D getSceneRenderExtent() const { return sceneRenderExtent; }
        bool isFrameInProgress() const { return isFrameStarted; }

        void resizeSceneIfNeeded(uint32_t w, uint32_t h);

        VkCommandBuffer getCurrentCommandBuffer() const
        {
            assert(isFrameStarted && "Cannot get command buffer when frame not in progress.");
            return commandBuffers[currentFrameIndex];
        }

        int getFrameIndex() const
        {
            assert(isFrameStarted && "Cannot get frame index when frame not in progress.");
            return currentFrameIndex;
        }

        void initImGui(VkDescriptorPool descriptorPool);

    private:
        void createSceneRenderPass();
        void createSceneRenderTargets();
        void createSceneImages();
        void createSceneFramebuffers();
        void cleanupSceneRenderTargets();
        void createSceneViewportSampler();
        void destroySceneViewportSampler();
        void registerSceneViewportTextures();
        void unregisterSceneViewportTextures();
        void destroySceneRenderPass();
        VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);
        void createCommandBuffers();
        void freeCommandBuffers();
        void recreateSwapchain();
        void shutdownImGui();

        Window &window;
        VulkanDevice &vk_device;
        std::unique_ptr<VulkanSwapchain> vk_swapchain;
        VkRenderPass sceneRenderPass = VK_NULL_HANDLE;
        VkExtent2D sceneRenderExtent{};
        VkFormat sceneColorFormat = VK_FORMAT_UNDEFINED;
        VkFormat sceneDepthFormat = VK_FORMAT_UNDEFINED;
        VkSampler sceneViewportSampler = VK_NULL_HANDLE;
        std::vector<VkFramebuffer> sceneFramebuffers;
        std::vector<VkImage> sceneColorImages;
        std::vector<VkDeviceMemory> sceneColorImageMemorys;
        std::vector<VkImageView> sceneColorImageViews;
        std::vector<VkDescriptorSet> sceneViewportDescriptorSets;
        std::vector<VkImage> sceneDepthImages;
        std::vector<VkDeviceMemory> sceneDepthImageMemorys;
        std::vector<VkImageView> sceneDepthImageViews;
        std::vector<VkCommandBuffer> commandBuffers;

        uint32_t currentImageIndex;
        int currentFrameIndex = 0;
        bool isFrameStarted = false;
        bool imguiInitialized = false;
    };

}