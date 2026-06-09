#pragma once

#define GLFW_INCLUDE_VULKAN
#define VK_USER_PLATFORM_MACOS_MVK
#include <vulkan/vulkan.h>

// #define VMA_IMPLEMENTATION
// #include <vk_mem_alloc.h>

#include <stdio.h>
#include <stdlib.h>
#include <exception>
#include <iostream>
#include <optional>
#include <set>
#include <fstream>
#include <memory>
#include <string>
#include <unordered_map>

#include "Renderer/Material/Material.hpp"
#include "Platform/Window/Window.hpp"
#include "Renderer/Frame/FrameContext.hpp"
#include "Renderer/Resources/Model.hpp"
#include "Renderer/Scene/RenderScene.hpp"
#include "Renderer/Resources/Vertex.hpp"
#include "MaterialCache.hpp"
#include "vk_device.hpp"
#include "vk_pipeline.hpp"
#include "vk_types.hpp"

#include "Core/Logging/Logger.hpp"

namespace Faye
{

	class SimpleRenderSystem
	{
	public:
		SimpleRenderSystem(VulkanDevice &device, VkRenderPass renderPass, MaterialCache &materialCache, VkDescriptorSetLayout globalSetLayout, VkDescriptorSetLayout materialSetLayout);
		~SimpleRenderSystem();

		SimpleRenderSystem(const SimpleRenderSystem &) = delete;
		void operator=(const SimpleRenderSystem &) = delete;
		SimpleRenderSystem(SimpleRenderSystem &&) = delete;
		SimpleRenderSystem &operator=(SimpleRenderSystem &&) = delete;

		void invalidatePipelines(const std::string &compiledShader);
		void renderScene(FrameContext &frameContext, const RenderSceneSnapshot &renderScene);

                // Prepares the cached depth-prepass pipeline (call once after construction).
                void prepareDepthPrepassPipeline(VkRenderPass depthPrepassRenderPass,
                                                 VkDescriptorSetLayout globalSetLayout);
                // Renders all non-water renderables depth-only.
                void renderDepthPrepass(FrameContext &frameContext, const RenderSceneSnapshot &renderScene);

        private:
                struct MaterialPipelineKey
                {
                        std::string vertexShaderPath;
                        std::string fragmentShaderPath;
                        bool alphaBlend = false;

                        friend bool operator==(const MaterialPipelineKey &left, const MaterialPipelineKey &right) = default;
                };

                struct MaterialPipelineKeyHasher
                {
                        size_t operator()(const MaterialPipelineKey &key) const;
                };

                VulkanDevice &vk_device;
                VkRenderPass renderPass;
                MaterialCache &materialCache;
                std::unordered_map<MaterialPipelineKey, std::unique_ptr<VulkanPipeline>, MaterialPipelineKeyHasher> pipelineCache;

                VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};
                VkPipelineLayout depthPrepassPipelineLayout{VK_NULL_HANDLE};
                std::unique_ptr<VulkanPipeline> depthPrepassPipeline;

                void createPipelineLayout(VkDescriptorSetLayout globalSetLayout, VkDescriptorSetLayout materialSetLayout);
                VulkanPipeline &getOrCreatePipeline(const MaterialState &materialState);
                std::unique_ptr<VulkanPipeline> createPipeline(const MaterialPipelineKey &key) const;
                static MaterialPipelineKey makePipelineKey(const MaterialState &materialState);
                static std::string resolveCompiledShaderPath(const std::string &shaderPath);
        }; // class SimpleRenderSystem

} // namespace Faye
