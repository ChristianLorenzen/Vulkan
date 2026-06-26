#define GLFW_INCLUDE_VULKAN
#define VK_USER_PLATFORM_MACOS_MVK
#include <vulkan/vulkan.h>

#include "EditorRenderLayer.hpp"
#include "ImGuiRenderer.hpp"

#include <array>
#include <cassert>
#include <stdexcept>

using namespace Faye;

namespace
{
    constexpr uint32_t kImGuiDescriptorBudget = 256;

    size_t hashCombine(size_t seed, size_t value)
    {
        return seed ^ (value + 0x9e3779b9 + (seed << 6) + (seed >> 2));
    }

    // Selects the viewport descriptor set for the current debug mode. Mirrors the
    // former Vulkan::renderFrame getViewportDescriptorSet lambda.
    VkDescriptorSet pickViewportDescriptorSet(const ImGuiRenderer &imgui,
                                              uint32_t frameIndex,
                                              RenderDebugMode debugMode,
                                              std::optional<uint32_t> postProcessTarget)
    {
        switch (debugMode)
        {
        case RenderDebugMode::SceneColor:
            return imgui.getSceneColorDS(frameIndex);
        case RenderDebugMode::SceneDepth:
            return imgui.getSceneDepthDS(frameIndex);
        case RenderDebugMode::SceneMotion:
            return imgui.getSceneMotionDS(frameIndex);
        case RenderDebugMode::Lit:
            break;
        }

        // Lit: the final post-process target, or the raw scene color if none.
        if (postProcessTarget)
            return imgui.getPostProcessDS(*postProcessTarget, frameIndex);
        return imgui.getSceneColorDS(frameIndex);
    }
}

EditorRenderLayer::EditorRenderLayer() = default;

EditorRenderLayer::~EditorRenderLayer()
{
    shutdown();
}

size_t EditorRenderLayer::ThumbnailKeyHasher::operator()(const ThumbnailKey &key) const
{
    size_t hash = std::hash<VkImageView>{}(key.imageView);
    hash = hashCombine(hash, std::hash<VkSampler>{}(key.sampler));
    return hash;
}

void EditorRenderLayer::createDescriptorPool(VkDevice dev)
{
    const std::array<VkDescriptorPoolSize, 11> poolSizes{{
        {VK_DESCRIPTOR_TYPE_SAMPLER, kImGuiDescriptorBudget},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, kImGuiDescriptorBudget},
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, kImGuiDescriptorBudget},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, kImGuiDescriptorBudget},
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, kImGuiDescriptorBudget},
        {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, kImGuiDescriptorBudget},
        {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, kImGuiDescriptorBudget},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, kImGuiDescriptorBudget},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, kImGuiDescriptorBudget},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, kImGuiDescriptorBudget},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, kImGuiDescriptorBudget},
    }};

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets = kImGuiDescriptorBudget;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();

    if (vkCreateDescriptorPool(dev, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create ImGui descriptor pool");
    }
}

void EditorRenderLayer::destroyDescriptorPool()
{
    if (descriptorPool != VK_NULL_HANDLE && device != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorPool(device, descriptorPool, nullptr);
    }
    descriptorPool = VK_NULL_HANDLE;
}

void EditorRenderLayer::init(GLFWwindow *window, IRenderer &targets)
{
    // Single-shot: a second init would create a second pool/ImGui context,
    // leak the first, and trip ImGuiRenderer's own assert. Treat as a no-op.
    assert(!imgui && "EditorRenderLayer::init called twice");
    if (imgui)
        return;

    const RenderBackendHandles handles = targets.getBackendHandles();
    device = handles.device;

    createDescriptorPool(device);

    imgui = std::make_unique<ImGuiRenderer>();
    imgui->init(
        window,
        handles.instance,
        handles.physicalDevice,
        handles.device,
        handles.queueFamily,
        handles.queue,
        descriptorPool,
        handles.swapchainColorFormat,
        handles.minImageCount,
        handles.imageCount);
    imgui->registerViewportTextures(targets);
}

void EditorRenderLayer::shutdown()
{
    // Ensure no in-flight frame still references the ImGui resources we are
    // about to destroy. The renderer's device outlives this layer, so the
    // handle is valid here. (The former Vulkan facade waited idle before
    // tearing ImGui down; preserve that ordering.)
    if (device != VK_NULL_HANDLE)
    {
        vkDeviceWaitIdle(device);
    }

    if (!imgui)
    {
        destroyDescriptorPool();
        return;
    }

    clearThumbnails();
    imgui->shutdown();
    imgui.reset();
    destroyDescriptorPool();
}

void EditorRenderLayer::onSwapchainRecreated(GLFWwindow *window, IRenderer &targets)
{
    if (!imgui)
        return;

    const RenderBackendHandles handles = targets.getBackendHandles();

    // Drop cached thumbnail descriptors *before* shutting ImGui down: the sets
    // were allocated from our pool via AddTexture and would otherwise become
    // stale handles (and leak — ImGui_ImplVulkan_Shutdown does not free
    // user-pool sets). They re-register lazily on next access after re-init.
    clearThumbnails();

    // vkDeviceWaitIdle was already issued inside the renderer's swapchain
    // recreation, so destroying/recreating backend state here is safe.
    imgui->shutdown();
    imgui->init(
        window,
        handles.instance,
        handles.physicalDevice,
        handles.device,
        handles.queueFamily,
        handles.queue,
        descriptorPool,
        handles.swapchainColorFormat,
        handles.minImageCount,
        handles.imageCount);
    imgui->registerViewportTextures(targets);
}

void EditorRenderLayer::onSceneResized(IRenderer &targets)
{
    if (imgui)
        imgui->registerViewportTextures(targets);
}

void EditorRenderLayer::beginFrame()
{
    if (imgui)
        imgui->newFrame();
}

void EditorRenderLayer::buildViewportFrameData(IRenderer &targets,
                                               uint32_t frameIndex,
                                               RenderDebugMode debugMode,
                                               std::optional<uint32_t> postProcessTarget,
                                               ImGuiFrameData &out) const
{
    out.viewportDebugMode = debugMode;
    if (!imgui)
        return;

    out.viewportTexture = reinterpret_cast<ImTextureID>(
        pickViewportDescriptorSet(*imgui, frameIndex, debugMode, postProcessTarget));

    const VkExtent2D extent = targets.getSceneExtent();
    out.viewportSize = ImVec2(
        static_cast<float>(extent.width),
        static_cast<float>(extent.height));
}

void EditorRenderLayer::render(VkCommandBuffer commandBuffer)
{
    if (imgui)
        imgui->render(commandBuffer);
}

ImTextureID EditorRenderLayer::registerThumbnail(VkImageView imageView, VkSampler sampler)
{
    if (!imgui || imageView == VK_NULL_HANDLE || sampler == VK_NULL_HANDLE)
        return 0;

    const ThumbnailKey key{imageView, sampler};
    if (const auto it = thumbnailDescriptors.find(key); it != thumbnailDescriptors.end())
    {
        return reinterpret_cast<ImTextureID>(it->second);
    }

    const VkDescriptorSet descriptorSet = imgui->registerTexture(sampler, imageView);
    thumbnailDescriptors.emplace(key, descriptorSet);
    return reinterpret_cast<ImTextureID>(descriptorSet);
}

void EditorRenderLayer::clearThumbnails()
{
    if (imgui)
    {
        for (const auto &[key, descriptorSet] : thumbnailDescriptors)
        {
            (void)key;
            if (descriptorSet != VK_NULL_HANDLE)
                imgui->unregisterTexture(descriptorSet);
        }
    }
    thumbnailDescriptors.clear();
}
