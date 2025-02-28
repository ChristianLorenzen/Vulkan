#pragma once

#include "Window/Window.hpp"
#include "vk_device.hpp"
#include "vk_swapchain.hpp"

#include <memory>
#include <vector>

#include <cassert>

namespace Faye {
    class VulkanRenderer {
        
        public:
            VulkanRenderer(Window &window, VulkanDevice &device);
            ~VulkanRenderer();

            VulkanRenderer(const VulkanRenderer &) = delete;
            VulkanRenderer &operator=(const VulkanRenderer &) = delete;

            VkCommandBuffer beginFrame();
            void endFrame();
            
            void beginSwapchainRenderPass(VkCommandBuffer commandBuffer);
            void endSwapchainRenderPass(VkCommandBuffer commandBuffer);
            
            VkRenderPass getSwapChainRenderPass() const {return vk_swapchain->getRenderPass();}
            float getAspectRatio() const {return vk_swapchain->extentAspectRatio();}
            bool isFrameInProgress() const {return isFrameStarted;}


            VkCommandBuffer getCurrentCommandBuffer() const {
                assert(isFrameStarted && "Cannot get command buffer when frame not in progress.");
                return commandBuffers[currentFrameIndex];
            }

            int getFrameIndex() const {
                assert(isFrameStarted && "Cannot get frame index when frame not in progress.");
                return currentFrameIndex;
            }

            void initImGui(VkDescriptorPool descriptorPool);

        private:
            void createCommandBuffers();
            void freeCommandBuffers();
            void recreateSwapchain();

            Window &window;
            VulkanDevice &vk_device;
            std::unique_ptr<VulkanSwapchain> vk_swapchain;
            std::vector<VkCommandBuffer> commandBuffers;

            uint32_t currentImageIndex;
            int currentFrameIndex = 0;
            bool isFrameStarted = false;
    };   

}