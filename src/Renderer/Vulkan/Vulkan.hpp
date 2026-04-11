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
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "Platform/Window/Window.hpp"
#include "Renderer/Frame/FrameContext.hpp"
#include "Renderer/Resources/Model.hpp"
#include "Renderer/Scene/RenderScene.hpp"
#include "Renderer/View/RenderView.hpp"
#include "MaterialCache.hpp"
#include "TextureCache.hpp"
#include "VulkanBuffer.hpp"
#include "vk_render_system.hpp"
#include "point_light_render_system.hpp"
#include "vk_post_process.hpp"
#include "vk_device.hpp"
#include "vk_pipeline.hpp"
#include "vk_renderer.hpp"
#include "vk_types.hpp"
#include "vk_descriptors.hpp"

#include "Core/Logging/Logger.hpp"

namespace Faye
{
	struct ImGuiFrameData;

	struct VulkanFrameInput
	{
		const RenderView &renderView;
		const RenderSceneSnapshot &renderScene;
		const PostProcessStackComponent *postProcessStack = nullptr;
		int frameTimeMs = 0;
		int averageFps = 0;
	};

	using ImGuiFrameCallback = std::function<void(ImGuiFrameData &)>;

	class Vulkan
	{
	public:
		explicit Vulkan(Window &win);
		~Vulkan();

		Vulkan(const Vulkan &) = delete;
		void operator=(const Vulkan &) = delete;
		Vulkan(Vulkan &&) = delete;
		Vulkan &operator=(Vulkan &&) = delete;

		void renderFrame(const VulkanFrameInput &frameInput, const ImGuiFrameCallback &drawImGui);
		float getAspectRatio() const;
		VkExtent2D getSceneRenderExtent() const;
		VkDescriptorSet getMaterialTextureThumbnail(MaterialHandle handle, const Material &material, TextureType type);

		VulkanDevice *getVkDevice() { return vk_device.get(); }

		void notifyShaderRecompilation(const std::string &compiledShader);

	private:
		Window &window;
		std::unique_ptr<VulkanDevice> vk_device;
		std::unique_ptr<VulkanRenderer> vk_renderer;
		std::unique_ptr<VulkanDescriptorSetLayout> globalSetLayout{};
		std::unique_ptr<VulkanDescriptorSetLayout> materialSetLayout{};
		std::unique_ptr<TextureCache> textureCache{};
		std::unique_ptr<MaterialCache> materialCache{};
		std::unique_ptr<SimpleRenderSystem> simpleRenderSystem{};
		std::unique_ptr<PointLightRenderSystem> pointLightRenderSystem{};
		std::unique_ptr<PostProcessChain> postProcessChain{};

		std::unique_ptr<VulkanDescriptorPool> globalPool{};
		std::unique_ptr<VulkanDescriptorPool> materialPool{};
		// Descriptor pool passed to the ImGui initialization function.
		std::unique_ptr<VulkanDescriptorPool> imGUIPool{};
		std::vector<std::unique_ptr<VulkanBuffer>> uboBuffers;
		std::vector<VkDescriptorSet> globalDescriptorSets;

		struct TextureThumbnailCacheKey
		{
			VkImageView imageView = VK_NULL_HANDLE;
			VkSampler sampler = VK_NULL_HANDLE;

			friend bool operator==(const TextureThumbnailCacheKey &left, const TextureThumbnailCacheKey &right) = default;
		};

		struct TextureThumbnailCacheKeyHasher
		{
			size_t operator()(const TextureThumbnailCacheKey &key) const;
		};

		std::unordered_map<TextureThumbnailCacheKey, VkDescriptorSet, TextureThumbnailCacheKeyHasher> textureThumbnailDescriptors;
		uint32_t pendingViewportWidth = 0;
		uint32_t pendingViewportHeight = 0;

		void initializeFrameResources();
	};

} // namespace
