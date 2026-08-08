#pragma once

// Engine-facing renderer backend interface. The engine (and the editor, through
// it) drives the frame through this interface instead of the concrete
// Faye::Vulkan facade, so engine/editor code no longer depends on the renderer's
// implementation headers. See ENGINE_CLEANUP Phase 3.

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <vulkan/vulkan.h>

#include "Core/Handles/MaterialHandle.hpp"
#include "Core/Handles/TextureType.hpp"
#include "Renderer/IRenderer.hpp"
#include "Renderer/Material/Material.hpp"
#include "Renderer/PostProcess/PostProcessEffectLibrary.hpp"
#include "Renderer/Scene/RenderScene.hpp"
#include "Renderer/View/RenderView.hpp"
#include "Renderer/Vulkan/profiler/vk_profiler.hpp"

namespace Faye
{
    // Identifies an in-flight frame across the engine-driven phase calls.
    struct FrameToken
    {
        VkCommandBuffer cmd = VK_NULL_HANDLE;
        int frameIndex = 0;
        bool swapchainRecreated = false;
    };

    // GPU image + sampler pair for a material texture. The lookup half of the
    // former material-thumbnail query; the UI layer turns this into its own
    // texture descriptor for display in panels.
    struct MaterialTextureView
    {
        VkImageView imageView = VK_NULL_HANDLE;
        VkSampler sampler = VK_NULL_HANDLE;
    };

    // Per-frame input for the scene render pass: the view to render, the
    // extracted scene snapshot, the post-process stack, and frame timing.
    struct RenderFrameInput
    {
        const RenderView &renderView;
        const RenderSceneSnapshot &renderScene;
        const PostProcessStackComponent *postProcessStack = nullptr;
        int frameTimeMs = 0;
        int averageFps = 0;
    };

    class IRendererBackend
    {
    public:
        virtual ~IRendererBackend() = default;

        // --- frame phases, driven by the engine (or a runtime shell) ---
        // Apply a pending offscreen scene resize. Returns true if a resize
        // actually happened (so the caller can re-register viewport textures).
        virtual bool setSceneRenderSize(uint32_t width, uint32_t height) = 0;
        // Begin a frame; nullopt when the swapchain is unavailable this frame.
        virtual std::optional<FrameToken> beginFrame() = 0;
        // Depth prepass + scene pass + point lights + editor grid overlay +
        // post-process effects.
        virtual void renderScene(const FrameToken &token, const RenderFrameInput &frameInput) = 0;
        // Begin the swapchain (present) render pass.
        virtual void beginPresentPass(const FrameToken &token) = 0;
        // Composite the final post-processed scene into the swapchain image.
        virtual void compositeSceneToSwapchain(const FrameToken &token, const PostProcessStackComponent *stack) = 0;
        // End the swapchain render pass.
        virtual void endPresentPass(const FrameToken &token) = 0;
        // Submit + present.
        virtual void endFrame(const FrameToken &token) = 0;

        // Underlying render targets / backend handles for an external UI layer.
        virtual IRenderer &targets() = 0;
        virtual uint64_t getSwapchainGeneration() const = 0;
        // The post-process target that holds the final image to display, or
        // nullopt when no effects are enabled (display the raw scene color).
        virtual std::optional<uint32_t> finalPostProcessTarget(const PostProcessStackComponent *stack) const = 0;

        virtual VkExtent2D getSceneRenderExtent() const = 0;
        virtual MaterialTextureView getMaterialTexture(MaterialHandle handle, const Material &material, TextureType type) = 0;
        // Uploads (or fetches from the texture cache) a standalone texture — editor
        // icons and other UI imagery that is not owned by a material.
        virtual MaterialTextureView getOrCreateTexture(const Texture &texture) = 0;

        virtual VkImageView getWaterDebugImageView() const = 0;

        virtual const std::vector<Profiler::ResolvedScope> &getScopeData() const = 0;

        // Notifies the backend that a shader file was recompiled (hot reload).
        virtual void notifyShaderRecompilation(const std::string &compiledShader) = 0;
    };
} // namespace Faye
