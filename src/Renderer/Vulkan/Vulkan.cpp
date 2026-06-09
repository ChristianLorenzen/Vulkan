#define VK_USER_PLATFORM_MACOS_MVK
#include <vulkan/vulkan.h>
#include <stdio.h>
#include <unordered_map>
#include <vector>

#include "Vulkan.hpp"
#include "Renderer/Frame/ImGuiFrameData.hpp"
#include "Renderer/Resources/Vertex.hpp"
// ImGuiRenderer lives in the Editor layer; included here (the orchestration
// implementation) rather than in the Vulkan.hpp header to keep the public
// header free of Editor dependencies.
#include "Editor/ImGui/ImGuiRenderer.hpp"

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

    initializeFrameResources();

    imGuiRenderer = std::make_unique<ImGuiRenderer>();
    imGuiRenderer->init(
        window.getWindow(),
        vk_device->getInstance(),
        vk_device->getPhysicalDevice(),
        vk_device->getDevice(),
        vk_device->getGraphicsQueueFamilyIndex(),
        vk_device->getGraphicsQueue(),
        imGUIPool->getPool(),
        vk_renderer->getSwapChainRenderPass(),
        VulkanSwapchain::MAX_FRAMES_IN_FLIGHT,
        VulkanSwapchain::MAX_FRAMES_IN_FLIGHT);
    imGuiRenderer->registerViewportTextures(*vk_renderer);
    lastImGuiSwapchainGeneration = vk_renderer->getSwapchainGeneration();
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
                          .addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
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

    // Depth sampler for contact-foam in water.frag (set=0, binding=1).
    // We always sample the OTHER frame's depth (previous frame completed, so it
    // is safely in DEPTH_STENCIL_READ_ONLY_OPTIMAL by the time we read it).
    if (sceneDepthSampler == VK_NULL_HANDLE)
    {
        VkSamplerCreateInfo depthSamplerInfo{};
        depthSamplerInfo.sType            = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        depthSamplerInfo.magFilter        = VK_FILTER_NEAREST;
        depthSamplerInfo.minFilter        = VK_FILTER_NEAREST;
        depthSamplerInfo.addressModeU     = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        depthSamplerInfo.addressModeV     = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        depthSamplerInfo.addressModeW     = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        depthSamplerInfo.anisotropyEnable = VK_FALSE;
        depthSamplerInfo.maxAnisotropy    = 1.0f;
        depthSamplerInfo.compareEnable    = VK_FALSE;
        depthSamplerInfo.compareOp        = VK_COMPARE_OP_ALWAYS;
        depthSamplerInfo.mipmapMode       = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        depthSamplerInfo.borderColor      = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
        if (vkCreateSampler(vk_device->getDevice(), &depthSamplerInfo, nullptr, &sceneDepthSampler) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create scene depth sampler");
        }
    }

    // Bind the depth PREPASS image for the same frame index.
    // The prepass runs before the scene pass each frame, writing opaque-only
    // depth to depthPrepassResources[i]. The scene pass then reads it via
    // set=0 binding=1. Using the same-frame index is safe because:
    //   1. The prepass render pass has an outgoing dependency that makes depth
    //      writes visible to subsequent fragment shader reads.
    //   2. The prepass image (depthPrepassResources) is separate from the
    //      scene depth (sceneDepthResources) so there is no read-write hazard.
    const auto prepassViews = vk_renderer->getDepthPrepassImageViews();
    globalDescriptorSets.resize(VulkanSwapchain::MAX_FRAMES_IN_FLIGHT);
    for (int i = 0; i < static_cast<int>(globalDescriptorSets.size()); i++)
    {
        auto bufferInfo = uboBuffers[i]->descriptorInfo();

        VkDescriptorImageInfo depthInfo{};
        depthInfo.sampler     = sceneDepthSampler;
        depthInfo.imageView   = prepassViews[i];
        depthInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

        VulkanDescriptorWriter(*globalSetLayout, *globalPool)
            .writeBuffer(0, &bufferInfo)
            .writeImage(1, &depthInfo)
            .build(globalDescriptorSets[i]);
    }

    simpleRenderSystem = std::make_unique<SimpleRenderSystem>(
        *vk_device,
        vk_renderer->getSceneRenderPass(),
        *materialCache,
        globalSetLayout->getDescriptorSetLayout(),
        materialSetLayout->getDescriptorSetLayout());

    simpleRenderSystem->prepareDepthPrepassPipeline(
        vk_renderer->getDepthPrepassRenderPass(),
        globalSetLayout->getDescriptorSetLayout());

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
            imGuiRenderer->unregisterTexture(descriptorSet);
        }
    }
    textureThumbnailDescriptors.clear();

    imGuiRenderer->shutdown();

    simpleRenderSystem.reset();
    pointLightRenderSystem.reset();
    postProcessChain.reset();
    materialCache.reset();
    textureCache.reset();
    if (sceneDepthSampler != VK_NULL_HANDLE)
    {
        vkDestroySampler(vk_device->getDevice(), sceneDepthSampler, nullptr);
        sceneDepthSampler = VK_NULL_HANDLE;
    }
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

    const VkDescriptorSet descriptorSet = imGuiRenderer->registerTexture(
        texture->samplerResource.sampler,
        texture->imageResource.imageView);
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
    auto getViewportDescriptorSet = [&](int frameIndex) -> VkDescriptorSet
    {
        const auto fi = static_cast<uint32_t>(frameIndex);
        switch (frameInput.renderView.debugMode)
        {
        case RenderDebugMode::Lit:
        {
            const uint32_t finalTarget = postProcessChain->getFinalTargetIndex(frameInput.postProcessStack);
            if (finalTarget == VulkanRenderer::kPostProcessTargetCount)
                return imGuiRenderer->getSceneColorDS(fi);
            return imGuiRenderer->getPostProcessDS(finalTarget, fi);
        }
        case RenderDebugMode::SceneColor:
            return imGuiRenderer->getSceneColorDS(fi);
        case RenderDebugMode::SceneDepth:
            return imGuiRenderer->getSceneDepthDS(fi);
        case RenderDebugMode::SceneMotion:
            return imGuiRenderer->getSceneMotionDS(fi);
        }

        const uint32_t finalTarget = postProcessChain->getFinalTargetIndex(frameInput.postProcessStack);
        if (finalTarget == VulkanRenderer::kPostProcessTargetCount)
            return imGuiRenderer->getSceneColorDS(fi);
        return imGuiRenderer->getPostProcessDS(finalTarget, fi);
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
        if (vk_renderer->resizeSceneIfNeeded(pendingViewportWidth, pendingViewportHeight))
        {
            imGuiRenderer->registerViewportTextures(*vk_renderer);
            // Depth prepass images are recreated on resize -- update binding=1 descriptors.
            const auto prepassViews = vk_renderer->getDepthPrepassImageViews();
            for (int i = 0; i < static_cast<int>(globalDescriptorSets.size()); i++)
            {
                VkDescriptorImageInfo depthInfo{};
                depthInfo.sampler     = sceneDepthSampler;
                depthInfo.imageView   = prepassViews[i];
                depthInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
                VulkanDescriptorWriter(*globalSetLayout, *globalPool)
                    .writeImage(1, &depthInfo)
                    .overwrite(globalDescriptorSets[i]);
            }
        }
        pendingViewportWidth = 0;
        pendingViewportHeight = 0;
    }

    postProcessChain->syncResources();

    if (auto commandBuffer = vk_renderer->beginFrame())
    {
        int frameIndex = vk_renderer->getFrameIndex();

        // Detect swapchain recreation (e.g. window resize) and reinit ImGui with
        // the new render pass. vkDeviceWaitIdle was already called inside
        // recreateSwapchain(), so it is safe to destroy/recreate backend state here.
        const uint64_t currentSwapGen = vk_renderer->getSwapchainGeneration();
        if (currentSwapGen != lastImGuiSwapchainGeneration)
        {
            imGuiRenderer->shutdown();
            imGuiRenderer->init(
                window.getWindow(),
                vk_device->getInstance(),
                vk_device->getPhysicalDevice(),
                vk_device->getDevice(),
                vk_device->getGraphicsQueueFamilyIndex(),
                vk_device->getGraphicsQueue(),
                imGUIPool->getPool(),
                vk_renderer->getSwapChainRenderPass(),
                VulkanSwapchain::MAX_FRAMES_IN_FLIGHT,
                VulkanSwapchain::MAX_FRAMES_IN_FLIGHT);
            imGuiRenderer->registerViewportTextures(*vk_renderer);
            lastImGuiSwapchainGeneration = currentSwapGen;
        }

        FrameContext frameContext{
            frameIndex,
            static_cast<float>(frameInput.frameTimeMs),
            commandBuffer,
            *primaryCamera,
            globalDescriptorSets[frameIndex]};

        totalElapsedTime += static_cast<float>(frameInput.frameTimeMs) / 1000.0f;

        GlobalUBO ubo{};
        ubo.priorViewProjection = primaryCamera->getPriorViewProjection();
        ubo.projection = primaryCamera->getProjection();
        ubo.view = primaryCamera->getView();
        ubo.inverseView = primaryCamera->getInverseView();
        ubo.time = totalElapsedTime;
        ubo.deltaTime = static_cast<float>(frameInput.frameTimeMs) / 1000.0f;
        ubo.inverseProjection = primaryCamera->getInverseProjection();

        // Update point light system UBO with the point light data from this frame's render scene.
        pointLightRenderSystem->update(frameContext, frameInput.renderScene, ubo);

        uboBuffers[frameIndex]->writeToBuffer(&ubo);
        uboBuffers[frameIndex]->flush();

        // Opaque depth prepass -- write depth for all non-water objects.
        // water.frag samples this via set=0 binding=1 (prepassDepth) to
        // compute contact/intersection foam without self-contamination.
        vk_renderer->beginDepthPrepassRenderPass(commandBuffer);
        simpleRenderSystem->renderDepthPrepass(frameContext, frameInput.renderScene);
        vk_renderer->endDepthPrepassRenderPass(commandBuffer);

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

        imGuiRenderer->newFrame();

        ImGuiFrameData frameData{};
        frameData.frameTimeMs = frameInput.frameTimeMs;
        frameData.averageFps = frameInput.averageFps;
        frameData.viewportDebugMode = frameInput.renderView.debugMode;

        VkExtent2D sceneRenderExtent = vk_renderer->getSceneRenderExtent();
        frameData.viewportTexture = reinterpret_cast<ImTextureID>(
            getViewportDescriptorSet(frameIndex));
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

        imGuiRenderer->render(commandBuffer);

        vk_renderer->endSwapchainRenderPass(commandBuffer);
        vk_renderer->endFrame();
    }
}