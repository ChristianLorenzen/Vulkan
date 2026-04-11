#define VK_USER_PLATFORM_MACOS_MVK
#include <vulkan/vulkan.h>
#include <stdio.h>
#include <unordered_map>
#include <vector>

#include "Vulkan.hpp"
#include "Renderer/Frame/ImGuiFrameData.hpp"
#include "Renderer/Resources/Vertex.hpp"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_vulkan.h"

#include "quill/LogMacros.h"

using namespace Faye;

const std::string MODEL_PATH = "src/include/viking_room.obj";
namespace
{
    constexpr uint32_t kImGuiDescriptorBudget = 256;

    size_t hashCombine(size_t seed, size_t value)
    {
        return seed ^ (value + 0x9e3779b9 + (seed << 6) + (seed >> 2));
    }
}

Faye::Vulkan::Vulkan(Window &win) : window{win}
{
    LOG_INFO(Logger::getInstance(), "Creating Vulkan Device class instance...");

    vk_device = std::make_unique<VulkanDevice>(window);

    vk_renderer = std::make_unique<VulkanRenderer>(window, *vk_device);
    globalPool = VulkanDescriptorPool::Builder(*vk_device)
                     .setMaxSets(VulkanSwapchain::MAX_FRAMES_IN_FLIGHT * 8)
                     .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VulkanSwapchain::MAX_FRAMES_IN_FLIGHT)
                     .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VulkanSwapchain::MAX_FRAMES_IN_FLIGHT * 8)
                     .setPoolFlags(VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT)
                     .build();

    materialPool = VulkanDescriptorPool::Builder(*vk_device)
                       .setMaxSets(1000)
                       .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 * 5)
                       .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000)
                       .setPoolFlags(VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT)
                       .build();

    imGUIPool = VulkanDescriptorPool::Builder(*vk_device)
                    .setMaxSets(kImGuiDescriptorBudget)
                    .addPoolSize(VK_DESCRIPTOR_TYPE_SAMPLER, kImGuiDescriptorBudget)
                    .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, kImGuiDescriptorBudget)
                    .addPoolSize(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, kImGuiDescriptorBudget)
                    .addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, kImGuiDescriptorBudget)
                    .addPoolSize(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, kImGuiDescriptorBudget)
                    .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, kImGuiDescriptorBudget)
                    .addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, kImGuiDescriptorBudget)
                    .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, kImGuiDescriptorBudget)
                    .addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, kImGuiDescriptorBudget)
                    .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, kImGuiDescriptorBudget)
                    .addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, kImGuiDescriptorBudget)
                    .setPoolFlags(VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT)
                    .build();

    vk_renderer->initImGui(imGUIPool->getPool());
    initializeFrameResources();
}

void Faye::Vulkan::initializeFrameResources()
{
    uboBuffers.resize(VulkanSwapchain::MAX_FRAMES_IN_FLIGHT);
    for (int i = 0; i < uboBuffers.size(); i++)
    {
        uboBuffers[i] = std::make_unique<VulkanBuffer>(
            *vk_device,
            sizeof(GlobalUBO),
            1,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
        uboBuffers[i]->map();
    }

    globalSetLayout = VulkanDescriptorSetLayout::Builder(*vk_device)
                          .addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS)
                          .build();

    materialSetLayout = VulkanDescriptorSetLayout::Builder(*vk_device)
                            .addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
                            .addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
                            .addBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
                            .addBinding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
                            .addBinding(4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
                            .addBinding(5, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT)
                            .build();

    textureCache = std::make_unique<TextureCache>(*vk_device);

    materialCache = std::make_unique<MaterialCache>(
        *vk_device,
        *textureCache,
        *materialSetLayout,
        *materialPool);

    globalDescriptorSets.resize(VulkanSwapchain::MAX_FRAMES_IN_FLIGHT);
    for (int i = 0; i < globalDescriptorSets.size(); i++)
    {
        auto bufferInfo = uboBuffers[i]->descriptorInfo();
        VulkanDescriptorWriter(*globalSetLayout, *globalPool)
            .writeBuffer(0, &bufferInfo)
            .build(globalDescriptorSets[i]);
    }

    simpleRenderSystem = std::make_unique<SimpleRenderSystem>(
        *vk_device,
        vk_renderer->getSceneRenderPass(),
        *materialCache,
        globalSetLayout->getDescriptorSetLayout(),
        materialSetLayout->getDescriptorSetLayout());

    pointLightRenderSystem = std::make_unique<PointLightRenderSystem>(
        *vk_device,
        vk_renderer->getSceneRenderPass(),
        globalSetLayout->getDescriptorSetLayout());

    postProcessChain = std::make_unique<PostProcessChain>(
        *vk_device,
        *vk_renderer,
        *globalPool);
}

Faye::Vulkan::~Vulkan()
{
    if (vk_device != nullptr)
    {
        vkDeviceWaitIdle(vk_device->getDevice());
    }

    for (const auto &[key, descriptorSet] : textureThumbnailDescriptors)
    {
        (void)key;
        if (descriptorSet != VK_NULL_HANDLE)
        {
            ImGui_ImplVulkan_RemoveTexture(descriptorSet);
        }
    }
    textureThumbnailDescriptors.clear();

    simpleRenderSystem.reset();
    pointLightRenderSystem.reset();
    postProcessChain.reset();
    materialCache.reset();
    textureCache.reset();
    vk_renderer.reset();
    globalSetLayout.reset();
    materialSetLayout.reset();
    uboBuffers.clear();
    globalDescriptorSets.clear();
    imGUIPool.reset();
    materialPool.reset();
    globalPool.reset();
    vk_device.reset();
}

float Faye::Vulkan::getAspectRatio() const
{
    return vk_renderer->getAspectRatio();
}

VkExtent2D Faye::Vulkan::getSceneRenderExtent() const
{
    return vk_renderer->getSceneRenderExtent();
}

size_t Faye::Vulkan::TextureThumbnailCacheKeyHasher::operator()(const TextureThumbnailCacheKey &key) const
{
    size_t hash = std::hash<VkImageView>{}(key.imageView);
    hash = hashCombine(hash, std::hash<VkSampler>{}(key.sampler));
    return hash;
}

VkDescriptorSet Faye::Vulkan::getMaterialTextureThumbnail(MaterialHandle handle, const Material &material, TextureType type)
{
    if (!handle.isValid() || materialCache == nullptr)
    {
        return VK_NULL_HANDLE;
    }

    const MaterialState &materialState = materialCache->getOrCreateState(handle, material);
    const VkTextureResource *texture = materialState.findTexture(type);
    if (texture == nullptr || !texture->isValid())
    {
        return VK_NULL_HANDLE;
    }

    const TextureThumbnailCacheKey cacheKey{
        texture->imageResource.imageView,
        texture->samplerResource.sampler};

    if (const auto iterator = textureThumbnailDescriptors.find(cacheKey);
        iterator != textureThumbnailDescriptors.end())
    {
        return iterator->second;
    }

    const VkDescriptorSet descriptorSet = ImGui_ImplVulkan_AddTexture(
        texture->samplerResource.sampler,
        texture->imageResource.imageView,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    textureThumbnailDescriptors.emplace(cacheKey, descriptorSet);
    return descriptorSet;
}

void Faye::Vulkan::notifyShaderRecompilation(const std::string &compiledShader)
{
    simpleRenderSystem->invalidatePipelines(compiledShader);
    postProcessChain->invalidatePipelines(compiledShader);
}

void Faye::Vulkan::renderFrame(const VulkanFrameInput &frameInput, const ImGuiFrameCallback &drawImGui)
{
    auto getViewportDescriptorSet = [&]() -> VkDescriptorSet
    {
        switch (frameInput.renderView.debugMode)
        {
        case RenderDebugMode::Lit:
            return postProcessChain->getViewportDescriptorSet(frameInput.postProcessStack);
        case RenderDebugMode::SceneColor:
            return vk_renderer->getSceneViewportDescriptorSet();
        case RenderDebugMode::SceneDepth:
            return vk_renderer->getSceneDepthViewportDescriptorSet();
        case RenderDebugMode::SceneMotion:
            return vk_renderer->getSceneMotionViewportDescriptorSet();
        }

        return postProcessChain->getViewportDescriptorSet(frameInput.postProcessStack);
    };

    const auto *primaryCamera = frameInput.renderView.camera;
    if (primaryCamera == nullptr)
    {
        throw std::runtime_error("Render view does not have a camera");
    }

    if (frameInput.renderView.outputTarget != RenderOutputTarget::Swapchain)
    {
        if (frameInput.renderView.outputTarget != RenderOutputTarget::OffscreenSceneColor)
        {
            throw std::runtime_error("Unsupported render output target");
        }
    }

    // Resize offscreen scene targets to whatever the viewport panel requested last frame.
    if (pendingViewportWidth > 0 && pendingViewportHeight > 0)
    {
        vk_renderer->resizeSceneIfNeeded(pendingViewportWidth, pendingViewportHeight);

        pendingViewportWidth = 0;
        pendingViewportHeight = 0;
    }

    postProcessChain->syncResources();

    if (auto commandBuffer = vk_renderer->beginFrame())
    {
        int frameIndex = vk_renderer->getFrameIndex();

        FrameContext frameContext{
            frameIndex,
            static_cast<float>(frameInput.frameTimeMs),
            commandBuffer,
            *primaryCamera,
            globalDescriptorSets[frameIndex]};

        GlobalUBO ubo{};
        ubo.priorViewProjection = primaryCamera->getPriorViewProjection();
        ubo.projection = primaryCamera->getProjection();
        ubo.view = primaryCamera->getView();
        ubo.inverseView = primaryCamera->getInverseView();

        // Update point light system UBO with the point light data from this frame's render scene.
        pointLightRenderSystem->update(frameContext, frameInput.renderScene, ubo);

        uboBuffers[frameIndex]->writeToBuffer(&ubo);
        uboBuffers[frameIndex]->flush();

        vk_renderer->beginSceneRenderPass(commandBuffer);

        simpleRenderSystem->renderScene(frameContext, frameInput.renderScene);

        if (!frameInput.renderScene.pointLights.empty())
        {
            pointLightRenderSystem->render(frameContext, frameInput.renderScene);
        }

        vk_renderer->endSceneRenderPass(commandBuffer);

        postProcessChain->renderEffects(frameContext, frameInput.postProcessStack);

        vk_renderer->beginSwapchainRenderPass(commandBuffer);

        postProcessChain->renderComposite(frameContext, frameInput.postProcessStack);

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGuiFrameData frameData{};
        frameData.frameTimeMs = frameInput.frameTimeMs;
        frameData.averageFps = frameInput.averageFps;
        frameData.viewportDebugMode = frameInput.renderView.debugMode;

        VkExtent2D sceneRenderExtent = vk_renderer->getSceneRenderExtent();
        frameData.viewportTexture = reinterpret_cast<ImTextureID>(
            getViewportDescriptorSet());
        frameData.viewportSize = ImVec2(
            static_cast<float>(sceneRenderExtent.width),
            static_cast<float>(sceneRenderExtent.height));

        if (drawImGui)
        {
            drawImGui(frameData);
        }

        // Save the viewport size requested this frame; resize will happen at the next frame start.
        if (frameData.requestedViewportSize.x > 0.0f && frameData.requestedViewportSize.y > 0.0f)
        {
            pendingViewportWidth = static_cast<uint32_t>(frameData.requestedViewportSize.x);
            pendingViewportHeight = static_cast<uint32_t>(frameData.requestedViewportSize.y);
        }

        ImGui::Render();
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer, 0);

        vk_renderer->endSwapchainRenderPass(commandBuffer);
        vk_renderer->endFrame();
    }
}