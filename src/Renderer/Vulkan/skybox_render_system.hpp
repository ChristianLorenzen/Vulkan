#pragma once

#define VK_USER_PLATFORM_MACOS_MVK
#include <vulkan/vulkan.h>

#include <memory>

#include "Renderer/Frame/FrameContext.hpp"
#include "Renderer/View/RenderView.hpp"
#include "vk_descriptors.hpp"
#include "vk_device.hpp"
#include "vk_pipeline.hpp"

namespace Faye {
    class SkyboxRenderSystem {
        public:
            SkyboxRenderSystem(VulkanDevice &device,
							   VkFormat colorFormat,
							   VkFormat motionFormat,
							   VkFormat depthFormat,
							   VulkanDescriptorSetLayout &globalSetLayout);
            ~SkyboxRenderSystem();

            SkyboxRenderSystem(const SkyboxRenderSystem &) = delete;
            void operator=(const SkyboxRenderSystem &) = delete;
            SkyboxRenderSystem(SkyboxRenderSystem &&) = delete;
            SkyboxRenderSystem &operator=(SkyboxRenderSystem &&) = delete;

            void render(FrameContext &frameContext, const SkyboxSettings &settings);

        private:
        	void createPipelineLayout(VkDescriptorSetLayout globalSetLayout);
            void createPipeline(VkFormat colorFormat, VkFormat motionFormat, VkFormat depthFormat);

            VulkanDevice &vk_device;
            VulkanDescriptorSetLayout &globalDescriptorSetLayout;
            std::unique_ptr<VulkanPipeline> vk_pipeline;
            VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};
    };
}