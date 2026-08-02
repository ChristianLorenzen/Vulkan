#pragma once

#define VK_USER_PLATFORM_MACOS_MVK
#include <vulkan/vulkan.h>

#include <glm/glm.hpp>

#include <stdio.h>
#include <stdlib.h>
#include <exception>
#include <iostream>
#include <optional>
#include <set>
#include <fstream>
#include <unordered_map>
#include <memory>

#include "Platform/Window/Window.hpp"
#include "Renderer/Frame/FrameContext.hpp"
#include "Renderer/PostProcess/PostProcessEffectLibrary.hpp"
#include "Renderer/Resources/Model.hpp"
#include "Renderer/Scene/RenderScene.hpp"
#include "Renderer/Resources/Vertex.hpp"
#include "vk_descriptors.hpp"
#include "vk_device.hpp"
#include "vk_pipeline.hpp"
#include "vk_types.hpp"

#include "Core/Logging/Logger.hpp"

namespace Faye
{
    class VulkanRenderer;

    class FullscreenEffectPass
    {
    public:
        FullscreenEffectPass(
            VulkanDevice &device,
            VkFormat colorFormat,
            VulkanDescriptorPool &descriptorPool,
            PostProcessEffectDefinition effectDefinition);
        ~FullscreenEffectPass();

        FullscreenEffectPass(const FullscreenEffectPass &) = delete;
        void operator=(const FullscreenEffectPass &) = delete;
        FullscreenEffectPass(FullscreenEffectPass &&) = delete;
        FullscreenEffectPass &operator=(FullscreenEffectPass &&) = delete;

        void invalidatePipelines(const std::string &compiledShader);
        void render(
            FrameContext &frameContext,
            const std::unordered_map<std::string, std::vector<VkImageView>> &namedInputs,
            const PostProcessParameterBlock &parameters);

    private:
        VulkanDevice &vk_device;
        PostProcessEffectDefinition effectDefinition;
        std::unique_ptr<VulkanPipeline> vk_pipeline;
        VulkanDescriptorPool &descriptorPool;
        VkFormat colorFormat;
        VkSampler sceneColorSampler{VK_NULL_HANDLE};
        std::vector<VkDescriptorSet> descriptorSets;

        VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};
        std::unique_ptr<VulkanDescriptorSetLayout> descriptorSetLayout;

        void createDescriptorSetLayout();
        void createSceneColorSampler();
        void createPipelineLayout();
        void createPipeline();
        void updateDescriptorSet(
            uint32_t frameIndex,
            const std::unordered_map<std::string, std::vector<VkImageView>> &namedInputs);
    };

    class PostProcessChain
    {
    public:
        PostProcessChain(VulkanDevice &device, VulkanRenderer &renderer, VulkanDescriptorPool &descriptorPool);
        ~PostProcessChain() = default;

        PostProcessChain(const PostProcessChain &) = delete;
        void operator=(const PostProcessChain &) = delete;
        PostProcessChain(PostProcessChain &&) = delete;
        PostProcessChain &operator=(PostProcessChain &&) = delete;

        void syncResources();
        void renderEffects(FrameContext &frameContext, const PostProcessStackComponent *stack);
        void renderComposite(FrameContext &frameContext, const PostProcessStackComponent *stack);
        bool hasEnabledEffects(const PostProcessStackComponent *stack) const;
        uint32_t getFinalTargetIndex(const PostProcessStackComponent *stack) const;
        void invalidatePipelines(const std::string &compiledShader);

    private:
        VulkanDevice &vk_device;
        VulkanRenderer &vk_renderer;
        VulkanDescriptorPool &descriptorPool;
        std::unordered_map<std::string, std::unique_ptr<FullscreenEffectPass>> effectPasses;
        std::unique_ptr<FullscreenEffectPass> presentPass;
        uint64_t swapchainGeneration = 0;

        void recreatePresentPass();
        uint32_t getEnabledEffectCount(const PostProcessStackComponent *stack) const;
        FullscreenEffectPass &getOrCreateEffectPass(const PostProcessEffectDefinition &definition);
        std::unordered_map<std::string, std::vector<VkImageView>> makeNamedInputViews(
            const std::vector<VkImageView> &sourceColorViews) const;
    };
}