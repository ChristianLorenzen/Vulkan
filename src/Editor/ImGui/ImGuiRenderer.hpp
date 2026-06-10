#pragma once

#include "Renderer/IRenderer.hpp"

#include <vector>

struct GLFWwindow;

namespace Faye
{
    // Owns the full ImGui lifecycle (init/shutdown/newFrame/render) and all
    // viewport texture descriptor sets that the editor uses to display render
    // targets. VulkanRenderer retains ownership of GPU resources (images,
    // sampler) but has no direct dependency on ImGui.
    class ImGuiRenderer
    {
    public:
        ImGuiRenderer() = default;
        ~ImGuiRenderer() = default;

        ImGuiRenderer(const ImGuiRenderer &) = delete;
        ImGuiRenderer &operator=(const ImGuiRenderer &) = delete;

        void init(GLFWwindow *window,
                  VkInstance instance,
                  VkPhysicalDevice physicalDevice,
                  VkDevice device,
                  uint32_t queueFamily,
                  VkQueue queue,
                  VkDescriptorPool descriptorPool,
                  VkFormat swapchainColorFormat,
                  uint32_t minImageCount,
                  uint32_t imageCount);

        void shutdown();

        void newFrame();
        void render(VkCommandBuffer commandBuffer);

        // Register/re-register all scene and post-process viewport textures.
        // Safe to call multiple times; existing registrations are cleaned up first.
        void registerViewportTextures(IRenderer &renderer);
        void unregisterViewportTextures();

        // Register/unregister individual textures (e.g. material thumbnails).
        // Returns VK_NULL_HANDLE if not initialized.
        VkDescriptorSet registerTexture(VkSampler sampler, VkImageView imageView);
        void unregisterTexture(VkDescriptorSet ds);

        VkDescriptorSet getSceneColorDS(uint32_t frameIdx) const;
        VkDescriptorSet getSceneMotionDS(uint32_t frameIdx) const;
        VkDescriptorSet getSceneDepthDS(uint32_t frameIdx) const;
        VkDescriptorSet getPostProcessDS(uint32_t targetIdx, uint32_t frameIdx) const;

        bool isInitialized() const { return initialized; }

    private:
        bool initialized = false;

        std::vector<VkDescriptorSet> sceneColorDescriptorSets;
        std::vector<VkDescriptorSet> sceneMotionDescriptorSets;
        std::vector<VkDescriptorSet> sceneDepthDescriptorSets;
        // Indexed as [targetIdx][frameIdx]
        std::vector<std::vector<VkDescriptorSet>> postProcessDescriptorSets;
    };
}
