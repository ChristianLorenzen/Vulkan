#pragma once

#include "Platform/Window/Window.hpp"
#include "vk_device.hpp"
#include "vk_render_pass.hpp"
#include "vk_swapchain.hpp"

#include <memory>
#include <array>
#include <vector>

#include <cassert>

namespace Faye
{
    class VulkanRenderer
    {
    public:
        static constexpr uint32_t kPostProcessTargetCount = 2;

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
        void beginPostProcessRenderPass(VkCommandBuffer commandBuffer, uint32_t targetIndex);
        void endPostProcessRenderPass(VkCommandBuffer commandBuffer);

        VkRenderPass getSceneRenderPass() const { return sceneRenderPass ? sceneRenderPass->getHandle() : VK_NULL_HANDLE; }
        VkRenderPass getSwapChainRenderPass() const { return vk_swapchain->getRenderPass(); }
        VkRenderPass getPostProcessRenderPass() const { return postProcessRenderPass->getHandle(); }
        float getAspectRatio() const { return vk_swapchain->extentAspectRatio(); }
        VkDescriptorSet getSceneViewportDescriptorSet() const { return sceneViewportDescriptorSets[currentFrameIndex]; }
        VkDescriptorSet getSceneMotionViewportDescriptorSet() const { return sceneMotionViewportDescriptorSets[currentFrameIndex]; }
        VkDescriptorSet getSceneDepthViewportDescriptorSet() const { return sceneDepthViewportDescriptorSets[currentFrameIndex]; }
        VkDescriptorSet getPostProcessViewportDescriptorSet(uint32_t targetIndex) const { return postProcessTargets.at(targetIndex).viewportDescriptorSets[currentFrameIndex]; }
        VkExtent2D getSceneRenderExtent() const { return sceneRenderExtent; }
        std::vector<VkImageView> getSceneImageViews() const { return sceneColorImageViews; }
        std::vector<VkImageView> getSceneMotionImageViews() const { return sceneMotionImageViews; }
        std::vector<VkImageView> getSceneDepthImageViews() const { return sceneDepthImageViews; }
        std::vector<VkImageView> getPostProcessTargetImageViews(uint32_t targetIndex) const { return postProcessTargets.at(targetIndex).imageViews; }
        VkImageView getSceneColorImageView(uint32_t index) const { return sceneColorImageViews[index]; }
        uint64_t getSwapchainGeneration() const { return swapchainGeneration; }
        bool isFrameInProgress() const { return isFrameStarted; }

        bool resizeSceneIfNeeded(uint32_t w, uint32_t h);

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
        struct PostProcessTargetResources
        {
            std::vector<VkImage> images;
            std::vector<VkDeviceMemory> imageMemories;
            std::vector<VkImageView> imageViews;
            std::vector<VkDescriptorSet> viewportDescriptorSets;
            std::vector<VulkanRenderPassInstance> renderPassInstances;
        };

        void createSceneRenderPass();
        void createPostProcessRenderPass();
        void createSceneRenderTargets();
        void createSceneImages();
        void createPostProcessImages();
        void createSceneFramebuffers();
        void createPostProcessFramebuffers();
        void cleanupSceneRenderTargets();
        void cleanupPostProcessRenderTargets();
        void createSceneViewportSampler();
        void destroySceneViewportSampler();
        void registerSceneViewportTextures();
        void unregisterSceneViewportTextures();
        void destroySceneRenderPass();
        void destroyPostProcessRenderPass();
        VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);
        void createCommandBuffers();
        void freeCommandBuffers();
        void recreateSwapchain();
        void shutdownImGui();

        Window &window;
        VulkanDevice &vk_device;
        std::unique_ptr<VulkanSwapchain> vk_swapchain;
        std::unique_ptr<VulkanRenderPass> sceneRenderPass;
        std::unique_ptr<VulkanRenderPass> postProcessRenderPass;
        VkExtent2D sceneRenderExtent{};
        VkFormat sceneColorFormat = VK_FORMAT_UNDEFINED;
        VkFormat sceneMotionFormat = VK_FORMAT_UNDEFINED;
        VkFormat sceneDepthFormat = VK_FORMAT_UNDEFINED;
        VkSampler sceneViewportSampler = VK_NULL_HANDLE;

        std::vector<VkImage> sceneColorImages;
        std::vector<VkDeviceMemory> sceneColorImageMemorys;
        std::vector<VkImageView> sceneColorImageViews;
        std::vector<VkDescriptorSet> sceneViewportDescriptorSets;
        std::vector<VkDescriptorSet> sceneMotionViewportDescriptorSets;
        std::vector<VkDescriptorSet> sceneDepthViewportDescriptorSets;

        std::vector<VkImage> sceneMotionImages;
        std::vector<VkDeviceMemory> sceneMotionImageMemorys;
        std::vector<VkImageView> sceneMotionImageViews;

        std::vector<VkImage> sceneDepthImages;
        std::vector<VkDeviceMemory> sceneDepthImageMemorys;
        std::vector<VkImageView> sceneDepthImageViews;

        std::vector<VulkanRenderPassInstance> sceneRenderPassInstances;
        std::array<PostProcessTargetResources, kPostProcessTargetCount> postProcessTargets;
        std::vector<VkCommandBuffer> commandBuffers;

        VkDescriptorPool imguiDescriptorPool = VK_NULL_HANDLE;
        uint32_t currentImageIndex;
        uint64_t swapchainGeneration = 0;
        int activePostProcessTargetIndex = -1;
        int currentFrameIndex = 0;
        bool isFrameStarted = false;
        bool imguiInitialized = false;
    };

}