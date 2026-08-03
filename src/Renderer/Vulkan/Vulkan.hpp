#pragma once

#define VK_USER_PLATFORM_MACOS_MVK
#include <vulkan/vulkan.h>

// #define VMA_IMPLEMENTATION
// #include <vk_mem_alloc.h>

#include <exception>
#include <optional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "Platform/Window/Window.hpp"
#include "Renderer/Frame/FrameContext.hpp"
#include "Renderer/IRenderer.hpp"
#include "Renderer/Resources/Model.hpp"
#include "Renderer/Scene/RenderScene.hpp"
#include "Renderer/View/RenderView.hpp"
#include "Renderer/Environment/environment_map.hpp"
#include "Renderer/Vulkan/profiler/vk_profiler.hpp"
#include "MaterialCache.hpp"
#include "TextureCache.hpp"
#include "VulkanBuffer.hpp"
#include "vk_render_system.hpp"
#include "point_light_render_system.hpp"
#include "editor_grid_render_system.hpp"
#include "skybox_render_system.hpp"
#include "vk_post_process.hpp"
#include "vk_device.hpp"
#include "vk_pipeline.hpp"
#include "vk_renderer.hpp"
#include "vk_types.hpp"
#include "vk_descriptors.hpp"
#include "vk_compute_pipeline.hpp"

#include "Core/Logging/Logger.hpp"

#include "vk_mem_alloc.h"

namespace Faye
{
	struct VulkanFrameInput
	{
		const RenderView &renderView;
		const RenderSceneSnapshot &renderScene;
		const PostProcessStackComponent *postProcessStack = nullptr;
		int frameTimeMs = 0;
		int averageFps = 0;
	};

	// GPU image + sampler pair for a material texture. The lookup half of the
	// former material-thumbnail query; the UI layer turns this into its own
	// texture descriptor for display in panels.
	struct MaterialTextureView
	{
		VkImageView imageView = VK_NULL_HANDLE;
		VkSampler sampler = VK_NULL_HANDLE;
	};

	class Vulkan
	{
	public:
		// Identifies an in-flight frame across the editor-driven phase calls.
		struct FrameToken
		{
			VkCommandBuffer cmd = VK_NULL_HANDLE;
			int frameIndex = 0;
			bool swapchainRecreated = false;
		};

		explicit Vulkan(Window &win);
		~Vulkan();

		Vulkan(const Vulkan &) = delete;
		void operator=(const Vulkan &) = delete;
		Vulkan(Vulkan &&) = delete;
		Vulkan &operator=(Vulkan &&) = delete;

		// --- frame phases, driven by the editor (or a future runtime shell) ---
		// Apply a pending offscreen scene resize. Returns true if a resize
		// actually happened (so the caller can re-register viewport textures).
		bool setSceneRenderSize(uint32_t width, uint32_t height);
		// Begin a frame; nullopt when the swapchain is unavailable this frame.
		std::optional<FrameToken> beginFrame();
		// Depth prepass + scene pass + point lights + editor grid overlay +
		// post-process effects.
		void renderScene(const FrameToken &token, const VulkanFrameInput &frameInput);
		// Begin the swapchain (present) render pass.
		void beginPresentPass(const FrameToken &token);
		// Composite the final post-processed scene into the swapchain image.
		void compositeSceneToSwapchain(const FrameToken &token, const PostProcessStackComponent *stack);
		// End the swapchain render pass.
		void endPresentPass(const FrameToken &token);
		// Submit + present.
		void endFrame(const FrameToken &token);

		// Underlying render targets / backend handles for an external UI layer.
		IRenderer &targets() { return *vk_renderer; }
		uint64_t getSwapchainGeneration() const { return vk_renderer->getSwapchainGeneration(); }
		// The post-process target that holds the final image to display, or
		// nullopt when no effects are enabled (display the raw scene color).
		std::optional<uint32_t> finalPostProcessTarget(const PostProcessStackComponent *stack) const;

		float getAspectRatio() const;
		VkExtent2D getSceneRenderExtent() const;
		MaterialTextureView getMaterialTexture(MaterialHandle handle, const Material &material, TextureType type);
		// Uploads (or fetches from the texture cache) a standalone texture — editor
		// icons and other UI imagery that is not owned by a material.
		MaterialTextureView getOrCreateTexture(const Texture &texture);

		VulkanDevice *getVkDevice() { return vk_device.get(); }

		void notifyShaderRecompilation(const std::string &compiledShader);

	private:
		void initializeFrameResources();

		Window &window;
		std::unique_ptr<VulkanDevice> vk_device;
		std::unique_ptr<VulkanRenderer> vk_renderer;

		std::unique_ptr<Profiler::VkProfiler> profiler;

		std::unique_ptr<VulkanDescriptorSetLayout> globalSetLayout{};
		std::unique_ptr<VulkanDescriptorSetLayout> materialSetLayout{};
		std::unique_ptr<VulkanDescriptorSetLayout> bindlessSetLayout{};
		std::unique_ptr<VulkanDescriptorSetLayout> waterFieldSetLayout{};

		std::unique_ptr<VulkanDescriptorPool> globalPool{};
		std::unique_ptr<VulkanDescriptorPool> materialPool{};
		std::unique_ptr<VulkanDescriptorPool> bindlessPool{};

		std::unique_ptr<TextureCache> textureCache{};
		std::unique_ptr<MaterialCache> materialCache{};
		
		std::unique_ptr<SimpleRenderSystem> simpleRenderSystem{};
		std::unique_ptr<PointLightRenderSystem> pointLightRenderSystem{};
		std::unique_ptr<VulkanComputePipeline> waterDebugGradient{};
		VkImageResource waterFieldDebugImage;
		EnvironmentMap environmentMap;
		// Editor-only overlay. Constructed unconditionally (one pipeline), but
		// only recorded when the RenderView opts in — see renderScene.
		std::unique_ptr<EditorGridRenderSystem> editorGridRenderSystem{};
		std::unique_ptr<SkyboxRenderSystem> skyboxRenderSystem{};
		std::unique_ptr<PostProcessChain> postProcessChain{};

		std::vector<std::unique_ptr<VulkanBuffer>> uboBuffers;
		std::vector<std::unique_ptr<VulkanBuffer>> lightingBuffers;
		VkDescriptorSet bindlessDescriptorSet{VK_NULL_HANDLE};

		// Frame context for the in-flight frame: built in renderScene and reused
		// by compositeSceneToSwapchain (both need the same UBO/camera/depth info).
		std::optional<FrameContext> currentFrame;

		// Debug-only guard that the caller drives the phases in the documented
		// order: beginFrame → renderScene → beginPresentPass → composite →
		// endPresentPass → endFrame. Asserts catch out-of-order/leaked phases.
		enum class FramePhase
		{
			Idle,
			Acquired,
			Scene,
			Present,
			PresentEnded
		};
		FramePhase framePhase = FramePhase::Idle;

		// Swapchain generation last reported to the caller via a FrameToken, so we
		// surface a recreation exactly once (even when it happened in the prior
		// frame's endFrame). Lets the UI layer reinitialize against the new
		// swapchain at the right moment.
		uint64_t lastReportedSwapchainGeneration = 0;

		float totalElapsedTime = 0.0f;
		VkSampler sceneDepthSampler = VK_NULL_HANDLE;
	}; // class Vulkan
} // namespace Faye
