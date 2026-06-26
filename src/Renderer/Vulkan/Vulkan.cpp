#define VK_USER_PLATFORM_MACOS_MVK
#include <vulkan/vulkan.h>
#include <stdio.h>
#include <unordered_map>
#include <vector>

#include "Vulkan.hpp"
#include "Renderer/Resources/Vertex.hpp"

#include "quill/LogMacros.h"

#include <cassert>

using namespace Faye;

const std::string MODEL_PATH = "src/include/viking_room.obj";

Faye::Vulkan::Vulkan(Window &win) : window{win}
{
    LOG_INFO(Logger::get(), "Creating Vulkan Device class instance...");

    vk_device = std::make_unique<VulkanDevice>(window);

    vk_renderer = std::make_unique<VulkanRenderer>(window, *vk_device);
    globalPool = VulkanDescriptorPool::Builder(*vk_device)
                     .setMaxSets(VulkanSwapchain::MAX_FRAMES_IN_FLIGHT * 8)
                     .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VulkanSwapchain::MAX_FRAMES_IN_FLIGHT)
                     .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VulkanSwapchain::MAX_FRAMES_IN_FLIGHT * 8)
                     .setPoolFlags(VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT)
                     .build();

    // Material pool: one UBO per material only (textures now live in the bindless set).
    materialPool = VulkanDescriptorPool::Builder(*vk_device)
                       .setMaxSets(1000)
                       .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000)
                       .setPoolFlags(VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT)
                       .build();

    constexpr uint32_t kMaxBindlessTextures = 4096;
    bindlessPool = VulkanDescriptorPool::Builder(*vk_device)
                       .setMaxSets(1)
                       .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, kMaxBindlessTextures)
                       .setPoolFlags(VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT)
                       .build();

    initializeFrameResources();

    lastReportedSwapchainGeneration = vk_renderer->getSwapchainGeneration();
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

    // Global set (set 0): push descriptor — written directly into the command buffer each frame.
    globalSetLayout = VulkanDescriptorSetLayout::Builder(*vk_device)
                          .setFlags(VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR)
                          .addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS)
                          .addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
                          .build();

    // Material set (set 1): parameter UBO only — textures are now in the bindless set.
    materialSetLayout = VulkanDescriptorSetLayout::Builder(*vk_device)
                            .addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT)
                            .build();

    // Bindless texture set (set 2): runtime-sized array of all scene textures.
    constexpr uint32_t kMaxBindlessTextures = 4096;
    constexpr VkDescriptorBindingFlags kBindlessFlags =
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
        VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT |
        VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;
    bindlessSetLayout = VulkanDescriptorSetLayout::Builder(*vk_device)
                            .setFlags(VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT)
                            .addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                        VK_SHADER_STAGE_FRAGMENT_BIT, kMaxBindlessTextures, kBindlessFlags)
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
        depthSamplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        depthSamplerInfo.magFilter = VK_FILTER_NEAREST;
        depthSamplerInfo.minFilter = VK_FILTER_NEAREST;
        depthSamplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        depthSamplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        depthSamplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        depthSamplerInfo.anisotropyEnable = VK_FALSE;
        depthSamplerInfo.maxAnisotropy = 1.0f;
        depthSamplerInfo.compareEnable = VK_FALSE;
        depthSamplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
        depthSamplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        depthSamplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
        if (vkCreateSampler(vk_device->getDevice(), &depthSamplerInfo, nullptr, &sceneDepthSampler) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create scene depth sampler");
        }
    }

    // Allocate the single bindless descriptor set for all scene textures.
    // VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT allows specifying the
    // actual count at allocation time (up to kMaxBindlessTextures).
    {
        VkDescriptorSetVariableDescriptorCountAllocateInfo varCountInfo{};
        varCountInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO;
        uint32_t varCount = kMaxBindlessTextures;
        varCountInfo.descriptorSetCount = 1;
        varCountInfo.pDescriptorCounts = &varCount;

        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = bindlessPool->getPool();
        allocInfo.descriptorSetCount = 1;
        VkDescriptorSetLayout bindlessLayout = bindlessSetLayout->getDescriptorSetLayout();
        allocInfo.pSetLayouts = &bindlessLayout;
        allocInfo.pNext = &varCountInfo;

        if (vkAllocateDescriptorSets(vk_device->getDevice(), &allocInfo, &bindlessDescriptorSet) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to allocate bindless descriptor set");
        }
    }

    // Tell TextureCache about the bindless set so new textures get registered automatically.
    textureCache->initBindless(bindlessDescriptorSet);

    simpleRenderSystem = std::make_unique<SimpleRenderSystem>(
        *vk_device,
        vk_renderer->getSceneColorFormat(),
        vk_renderer->getSceneMotionFormat(),
        vk_renderer->getSceneDepthFormat(),
        *materialCache,
        *globalSetLayout,
        materialSetLayout->getDescriptorSetLayout(),
        bindlessSetLayout->getDescriptorSetLayout());

    simpleRenderSystem->prepareDepthPrepassPipeline(
        vk_renderer->getSceneDepthFormat(),
        globalSetLayout->getDescriptorSetLayout());

    pointLightRenderSystem = std::make_unique<PointLightRenderSystem>(
        *vk_device,
        vk_renderer->getSceneColorFormat(),
        vk_renderer->getSceneMotionFormat(),
        vk_renderer->getSceneDepthFormat(),
        *globalSetLayout);

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
    bindlessSetLayout.reset();
    uboBuffers.clear();
    materialPool.reset();
    bindlessPool.reset();
    globalPool.reset();
    vk_device.reset();
}

float Faye::Vulkan::getAspectRatio() const
{
    return vk_renderer->getAspectRatio();
}

VkExtent2D Faye::Vulkan::getSceneRenderExtent() const
{
    return vk_renderer->getSceneExtent();
}

MaterialTextureView Faye::Vulkan::getMaterialTexture(MaterialHandle handle, const Material &material, TextureType type)
{
    if (!handle.isValid() || materialCache == nullptr)
    {
        return {};
    }

    const MaterialState &state = materialCache->getOrCreateState(handle, material);

    // Look up the texture resource directly from the bindless slot stored on the
    // material state — no pixel-data hashing, just an index into the slot→resource
    // reverse map built by TextureCache.
    uint32_t slot = UINT32_MAX;
    switch (type)
    {
    case TextureType::Albedo:
        slot = state.albedoSlot;
        break;
    case TextureType::Normal:
        slot = state.normalSlot;
        break;
    case TextureType::Metallic:
        slot = state.metallicSlot;
        break;
    case TextureType::Roughness:
        slot = state.roughnessSlot;
        break;
    case TextureType::AmbientOcclusion:
        slot = state.aoSlot;
        break;
    default:
        break;
    }

    const VkTextureResource *texture = textureCache->getResourceForSlot(slot);
    if (texture == nullptr || !texture->isValid())
    {
        return {};
    }

    return MaterialTextureView{
        texture->imageResource.imageView,
        texture->samplerResource.sampler};
}

std::optional<uint32_t> Faye::Vulkan::finalPostProcessTarget(const PostProcessStackComponent *stack) const
{
    const uint32_t target = postProcessChain->getFinalTargetIndex(stack);
    if (target == VulkanRenderer::kPostProcessTargetCount)
    {
        return std::nullopt; // no enabled effects → display the raw scene color
    }
    return target;
}

void Faye::Vulkan::notifyShaderRecompilation(const std::string &compiledShader)
{
    simpleRenderSystem->invalidatePipelines(compiledShader);
    postProcessChain->invalidatePipelines(compiledShader);
}

bool Faye::Vulkan::setSceneRenderSize(uint32_t width, uint32_t height)
{
    if (width == 0 || height == 0)
    {
        return false;
    }
    return vk_renderer->resizeSceneIfNeeded(width, height);
}

std::optional<Faye::Vulkan::FrameToken> Faye::Vulkan::beginFrame()
{
    assert(framePhase == FramePhase::Idle && "beginFrame called while a frame is already in progress");
    postProcessChain->syncResources();

    auto commandBuffer = vk_renderer->beginFrame();
    if (!commandBuffer)
    {
        // Swapchain was out-of-date at acquire; it has been recreated and this
        // frame is skipped. The generation change is reported on the next
        // successful frame so the caller can reinitialize backend state then.
        return std::nullopt;
    }

    FrameToken token{};
    token.cmd = commandBuffer;
    token.frameIndex = vk_renderer->getFrameIndex();
    // Report a swapchain recreation relative to the last frame we handed out.
    // Recreation usually happens in the previous frame's endFrame (window
    // resize/present out-of-date), so we track the generation persistently
    // rather than diffing across a single beginFrame call. vkDeviceWaitIdle was
    // already issued inside recreateSwapchain, so it is safe for the caller to
    // tear down/recreate external backend state for the new swapchain now.
    const uint64_t currentGeneration = vk_renderer->getSwapchainGeneration();
    token.swapchainRecreated = currentGeneration != lastReportedSwapchainGeneration;
    lastReportedSwapchainGeneration = currentGeneration;
    framePhase = FramePhase::Acquired;
    return token;
}

void Faye::Vulkan::renderScene(const FrameToken &token, const VulkanFrameInput &frameInput)
{
    assert(framePhase == FramePhase::Acquired && "renderScene must be called once, right after beginFrame");
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

    const VkCommandBuffer commandBuffer = token.cmd;
    const int frameIndex = token.frameIndex;

    // Build prepass depth info for push descriptors (queried fresh — handles resize automatically).
    VkDescriptorImageInfo prepassDepthInfo{};
    prepassDepthInfo.sampler = sceneDepthSampler;
    prepassDepthInfo.imageView = vk_renderer->getDepthPrepassImageViews()[frameIndex];
    prepassDepthInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

    // Built here and retained for compositeSceneToSwapchain (present pass).
    currentFrame.emplace(FrameContext{
        frameIndex,
        static_cast<float>(frameInput.frameTimeMs),
        commandBuffer,
        *primaryCamera,
        uboBuffers[frameIndex].get(),
        prepassDepthInfo});
    FrameContext &frameContext = *currentFrame;

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

    // Bind the bindless texture set (set 2) with the scene pipeline layout AFTER
    // the depth prepass. The depth prepass uses a separate pipeline layout (set 0
    // only), which disturbs previously-bound sets at higher indices. Binding here
    // ensures set 2 is valid for all subsequent scene draw calls.
    VkDescriptorSet bindlessSets[] = {bindlessDescriptorSet};
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                            simpleRenderSystem->getPipelineLayout(),
                            2, 1, bindlessSets, 0, nullptr);

    vk_renderer->beginSceneRenderPass(commandBuffer);

    simpleRenderSystem->renderScene(frameContext, frameInput.renderScene);

    if (!frameInput.renderScene.pointLights.empty())
    {
        pointLightRenderSystem->render(frameContext, frameInput.renderScene);
    }

    vk_renderer->endSceneRenderPass(commandBuffer);

    postProcessChain->renderEffects(frameContext, frameInput.postProcessStack);
    framePhase = FramePhase::Scene;
}

void Faye::Vulkan::beginPresentPass(const FrameToken &token)
{
    assert(framePhase == FramePhase::Scene && "beginPresentPass must follow renderScene");
    vk_renderer->beginSwapchainRenderPass(token.cmd);
    framePhase = FramePhase::Present;
}

void Faye::Vulkan::compositeSceneToSwapchain(const FrameToken &token, const PostProcessStackComponent *stack)
{
    (void)token;
    assert(framePhase == FramePhase::Present && "compositeSceneToSwapchain must be inside the present pass");
    if (!currentFrame)
    {
        throw std::runtime_error("compositeSceneToSwapchain called without an active frame");
    }
    postProcessChain->renderComposite(*currentFrame, stack);
}

void Faye::Vulkan::endPresentPass(const FrameToken &token)
{
    assert(framePhase == FramePhase::Present && "endPresentPass must follow beginPresentPass");
    vk_renderer->endSwapchainRenderPass(token.cmd);
    framePhase = FramePhase::PresentEnded;
}

void Faye::Vulkan::endFrame(const FrameToken &token)
{
    (void)token;
    assert(framePhase == FramePhase::PresentEnded && "endFrame must follow endPresentPass");
    vk_renderer->endFrame();
    currentFrame.reset();
    framePhase = FramePhase::Idle;
}