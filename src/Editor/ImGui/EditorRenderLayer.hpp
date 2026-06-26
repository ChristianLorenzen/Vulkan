#pragma once

#include <vulkan/vulkan.h>

#include <memory>
#include <optional>
#include <unordered_map>

#include "Renderer/IRenderer.hpp"
#include "Editor/ImGui/ImGuiFrameData.hpp"

struct GLFWwindow;

namespace Faye
{
    class ImGuiRenderer;

    // Editor-owned ImGui integration. Owns the ImGui descriptor pool, the
    // ImGuiRenderer, the viewport-texture registrations and the material
    // thumbnail descriptor cache — everything that used to live inside the
    // Vulkan facade. The renderer exposes only ImGui-agnostic handles
    // (RenderBackendHandles, image views) through IRenderer.
    class EditorRenderLayer
    {
    public:
        EditorRenderLayer();
        ~EditorRenderLayer();

        EditorRenderLayer(const EditorRenderLayer &) = delete;
        EditorRenderLayer &operator=(const EditorRenderLayer &) = delete;

        // Creates the ImGui descriptor pool, initializes ImGui against the
        // renderer's backend handles and registers the viewport textures.
        void init(GLFWwindow *window, IRenderer &targets);
        void shutdown();

        // Swapchain was recreated (e.g. OS window resize): tear ImGui down and
        // reinitialize it against the new swapchain, then re-register textures.
        void onSwapchainRecreated(GLFWwindow *window, IRenderer &targets);
        // Offscreen scene targets were resized (viewport panel drag): re-register
        // the viewport textures so the descriptor sets point at the new images.
        void onSceneResized(IRenderer &targets);

        // Begin an ImGui frame (ImGui_ImplVulkan/Glfw NewFrame + ImGui::NewFrame).
        void beginFrame();
        // Pick the viewport texture/size for this frame from the editor's debug
        // mode and the final post-process target (nullopt = raw scene color).
        void buildViewportFrameData(IRenderer &targets,
                                    uint32_t frameIndex,
                                    RenderDebugMode debugMode,
                                    std::optional<uint32_t> postProcessTarget,
                                    ImGuiFrameData &out) const;
        // Record the ImGui draw data into the present pass.
        void render(VkCommandBuffer commandBuffer);

        // Register (and cache) a material thumbnail texture for use in panels.
        ImTextureID registerThumbnail(VkImageView imageView, VkSampler sampler);

    private:
        void createDescriptorPool(VkDevice device);
        void destroyDescriptorPool();
        void clearThumbnails();

        std::unique_ptr<ImGuiRenderer> imgui;
        VkDevice device = VK_NULL_HANDLE;
        VkDescriptorPool descriptorPool = VK_NULL_HANDLE;

        struct ThumbnailKey
        {
            VkImageView imageView = VK_NULL_HANDLE;
            VkSampler sampler = VK_NULL_HANDLE;

            friend bool operator==(const ThumbnailKey &a, const ThumbnailKey &b) = default;
        };

        struct ThumbnailKeyHasher
        {
            size_t operator()(const ThumbnailKey &key) const;
        };

        std::unordered_map<ThumbnailKey, VkDescriptorSet, ThumbnailKeyHasher> thumbnailDescriptors;
    };
}
