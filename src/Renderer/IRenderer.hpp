#pragma once
// Public interface of the Faye renderer.
// Editor and engine code should depend on this interface rather than the
// concrete VulkanRenderer type to avoid coupling to internal renderer headers.

#include <vulkan/vulkan.h>
#include <vector>
#include <cstdint>

namespace Faye {
    // Everything an external UI layer needs to initialize its own GPU backend
    // (e.g. a UI library's Vulkan init) without depending on renderer internals.
    struct RenderBackendHandles {
        VkInstance instance = VK_NULL_HANDLE;
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
        VkDevice device = VK_NULL_HANDLE;
        uint32_t queueFamily = 0;
        VkQueue queue = VK_NULL_HANDLE;
        VkFormat swapchainColorFormat = VK_FORMAT_UNDEFINED;
        uint32_t minImageCount = 0;
        uint32_t imageCount = 0;
    };

    class IRenderer {
    public:
        virtual ~IRenderer() = default;

        // Frame lifecycle
        virtual VkCommandBuffer beginFrame() = 0;
        virtual void endFrame() = 0;

        // Render passes
        virtual void beginSceneRenderPass(VkCommandBuffer cmd) = 0;
        virtual void endSceneRenderPass(VkCommandBuffer cmd) = 0;
        virtual void beginSwapchainRenderPass(VkCommandBuffer cmd) = 0;
        virtual void endSwapchainRenderPass(VkCommandBuffer cmd) = 0;

        // Scene resize
        virtual bool resizeSceneIfNeeded(uint32_t w, uint32_t h) = 0;
        virtual VkExtent2D getSceneExtent() const = 0;

        // Resource accessors (for editor viewport display)
        virtual VkSampler getSceneViewportSampler() const = 0;
        virtual VkImageView getSceneColorImageView(uint32_t frameIndex) const = 0;
        virtual uint32_t getMaxFramesInFlight() const = 0;

        // Handles an external UI backend needs for one-time init.
        virtual RenderBackendHandles getBackendHandles() const = 0;
        // Monotonically increasing counter bumped on every swapchain recreation,
        // so an external UI layer can detect when it must reinitialize.
        virtual uint64_t getSwapchainGeneration() const = 0;

        // Multi-image accessors used to register external viewport textures
        virtual std::vector<VkImageView> getSceneImageViews() const = 0;
        virtual std::vector<VkImageView> getSceneMotionImageViews() const = 0;
        virtual std::vector<VkImageView> getSceneDepthImageViews() const = 0;
        virtual std::vector<VkImageView> getPostProcessTargetImageViews(uint32_t targetIndex) const = 0;
        virtual uint32_t getPostProcessTargetCount() const = 0;
    };

} // namespace Faye
