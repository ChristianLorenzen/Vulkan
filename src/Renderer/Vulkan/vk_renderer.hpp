#pragma once

#include "Renderer/IRenderer.hpp"
#include "Platform/Window/Window.hpp"
#include "vk_device.hpp"
#include "vk_render_pass.hpp"
#include "vk_swapchain.hpp"
#include "VkImageResource.hpp"

#include <memory>
#include <array>
#include <vector>

#include <cassert>

namespace Faye
{
    class VulkanRenderer : public IRenderer
    {
    public:
        static constexpr uint32_t kPostProcessTargetCount = 2;

        VulkanRenderer(Window &window, VulkanDevice &device);
        ~VulkanRenderer();

        VulkanRenderer(const VulkanRenderer &) = delete;
        VulkanRenderer &operator=(const VulkanRenderer &) = delete;

        VkCommandBuffer beginFrame() override;
        void endFrame() override;

        void beginSceneRenderPass(VkCommandBuffer commandBuffer) override;
        void endSceneRenderPass(VkCommandBuffer commandBuffer) override;
        void beginSwapchainRenderPass(VkCommandBuffer commandBuffer) override;
        void endSwapchainRenderPass(VkCommandBuffer commandBuffer) override;
        void beginDepthPrepassRenderPass(VkCommandBuffer commandBuffer);
        void endDepthPrepassRenderPass(VkCommandBuffer commandBuffer);
        void beginPostProcessRenderPass(VkCommandBuffer commandBuffer, uint32_t targetIndex);
        void endPostProcessRenderPass(VkCommandBuffer commandBuffer);

        VkRenderPass getDepthPrepassRenderPass() const { return depthPrepassRenderPass ? depthPrepassRenderPass->getHandle() : VK_NULL_HANDLE; }
        VkRenderPass getSceneRenderPass() const { return sceneRenderPass ? sceneRenderPass->getHandle() : VK_NULL_HANDLE; }
        VkRenderPass getSwapChainRenderPass() const { return vk_swapchain->getRenderPass(); }
        VkRenderPass getPostProcessRenderPass() const { return postProcessRenderPass->getHandle(); }
        float getAspectRatio() const { return vk_swapchain->extentAspectRatio(); }
        VkSampler getSceneViewportSampler() const override { return sceneViewportSampler; }
        VkExtent2D getSceneExtent() const override { return sceneRenderExtent; }
        VkExtent2D getSceneRenderExtent() const { return sceneRenderExtent; }
        std::vector<VkImageView> getSceneImageViews() const override;
        std::vector<VkImageView> getSceneMotionImageViews() const override;
        std::vector<VkImageView> getSceneDepthImageViews() const override;
        std::vector<VkImageView> getDepthPrepassImageViews() const;
        std::vector<VkImageView> getPostProcessTargetImageViews(uint32_t targetIndex) const override { return postProcessTargets.at(targetIndex).imageViews; }
        VkImageView getSceneColorImageView(uint32_t index) const override;
        uint64_t getSwapchainGeneration() const { return swapchainGeneration; }
        bool isFrameInProgress() const { return isFrameStarted; }
        uint32_t getMaxFramesInFlight() const override { return VulkanSwapchain::MAX_FRAMES_IN_FLIGHT; }
        uint32_t getPostProcessTargetCount() const override { return kPostProcessTargetCount; }

        bool resizeSceneIfNeeded(uint32_t w, uint32_t h) override;

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

    private:
        struct PostProcessTargetResources
        {
            std::vector<VkImage> images;
            std::vector<VkDeviceMemory> imageMemories;
            std::vector<VkImageView> imageViews;
            std::vector<VulkanRenderPassInstance> renderPassInstances;
        };

        void createDepthPrepassRenderPass();
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
        void destroyDepthPrepassRenderPass();
        void destroySceneRenderPass();
        void destroyPostProcessRenderPass();
        VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);
        void createCommandBuffers();
        void freeCommandBuffers();
        void recreateSwapchain();

        Window &window;
        VulkanDevice &vk_device;
        std::unique_ptr<VulkanSwapchain> vk_swapchain;
        std::unique_ptr<VulkanRenderPass> depthPrepassRenderPass;
        std::unique_ptr<VulkanRenderPass> sceneRenderPass;
        std::unique_ptr<VulkanRenderPass> postProcessRenderPass;
        VkExtent2D sceneRenderExtent{};
        VkFormat sceneColorFormat = VK_FORMAT_UNDEFINED;
        VkFormat sceneMotionFormat = VK_FORMAT_UNDEFINED;
        VkFormat sceneDepthFormat = VK_FORMAT_UNDEFINED;
        VkSampler sceneViewportSampler = VK_NULL_HANDLE;

        std::vector<VkImageResource> sceneColorResources;
        std::vector<VkImageResource> sceneMotionResources;
        std::vector<VkImageResource> sceneDepthResources;
        std::vector<VkImageResource> depthPrepassResources;

        std::vector<VulkanRenderPassInstance> depthPrepassRenderPassInstances;
        std::vector<VulkanRenderPassInstance> sceneRenderPassInstances;
        std::array<PostProcessTargetResources, kPostProcessTargetCount> postProcessTargets;
        std::vector<VkCommandBuffer> commandBuffers;

        uint32_t currentImageIndex;
        uint64_t swapchainGeneration = 0;
        int activePostProcessTargetIndex = -1;
        int currentFrameIndex = 0;
        bool isFrameStarted = false;
    };

}