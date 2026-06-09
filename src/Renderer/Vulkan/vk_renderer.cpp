#define VK_USER_PLATFORM_MACOS_MVK
#include <vulkan/vulkan.h>
#include <stdio.h>
#include <unordered_map>

#include "Vulkan.hpp"

#include "quill/LogMacros.h"

#include <array>
#include <chrono>
#include <fstream>

#include "vk_renderer.hpp"
using namespace Faye;

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

Faye::VulkanRenderer::VulkanRenderer(Window &win, VulkanDevice &device) : window{win}, vk_device{device}
{
    LOG_INFO(Logger::getInstance(), "Creating Vulkan Device class instance...");

    LOG_INFO(Logger::getInstance(), "Creating Vulkan Swapchain...");
    recreateSwapchain();
    LOG_INFO(Logger::getInstance(), "Creating Vulkan CommandBuffers...");
    createCommandBuffers();
}

Faye::VulkanRenderer::~VulkanRenderer()
{
    cleanupSceneRenderTargets();
    destroyDepthPrepassRenderPass();
    destroySceneViewportSampler();
    destroySceneRenderPass();
    destroyPostProcessRenderPass();
    freeCommandBuffers();
}

std::vector<VkImageView> Faye::VulkanRenderer::getSceneImageViews() const
{
    std::vector<VkImageView> views;
    views.reserve(sceneColorResources.size());
    for (const auto &resource : sceneColorResources)
    {
        views.push_back(resource.imageView);
    }
    return views;
}

VkImageView Faye::VulkanRenderer::getSceneColorImageView(uint32_t index) const
{
    return sceneColorResources.at(index).imageView;
}

std::vector<VkImageView> Faye::VulkanRenderer::getSceneMotionImageViews() const
{
    std::vector<VkImageView> views;
    views.reserve(sceneMotionResources.size());
    for (const auto &resource : sceneMotionResources)
    {
        views.push_back(resource.imageView);
    }
    return views;
}

std::vector<VkImageView> Faye::VulkanRenderer::getSceneDepthImageViews() const
{
    std::vector<VkImageView> views;
    views.reserve(sceneDepthResources.size());
    for (const auto &resource : sceneDepthResources)
    {
        views.push_back(resource.imageView);
    }
    return views;
}

std::vector<VkImageView> Faye::VulkanRenderer::getDepthPrepassImageViews() const
{
    std::vector<VkImageView> views;
    views.reserve(depthPrepassResources.size());
    for (const auto &resource : depthPrepassResources)
    {
        views.push_back(resource.imageView);
    }
    return views;
}

VkCommandBuffer Faye::VulkanRenderer::beginFrame()
{
    assert(!isFrameStarted && "Can't call beginFrame while already in progress.");

    auto result = vk_swapchain->acquireNextImage(&currentImageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        recreateSwapchain();
        return nullptr;
    }

    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
    {
        throw std::runtime_error("Failed to acquire swap chain image");
    }

    isFrameStarted = true;

    auto commandBuffer = getCurrentCommandBuffer();

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    // beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    // beginInfo.pInheritanceInfo = nullptr;

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to begin recording command buffer");
    }
    return commandBuffer;
}

void Faye::VulkanRenderer::endFrame()
{
    assert(isFrameStarted && "Can't call endFrame while frame is not in progress.");
    auto commandBuffer = getCurrentCommandBuffer();

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to record command buffer");
    }

    auto result = vk_swapchain->submitCommandBuffers(&commandBuffer, &currentImageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || window.wasWindowResized())
    {
        window.resetWindowResizedFlag();
        recreateSwapchain();
    }
    else if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to present swap chain image");
    }

    isFrameStarted = false;
    currentFrameIndex = (currentFrameIndex + 1) % VulkanSwapchain::MAX_FRAMES_IN_FLIGHT;
}

void Faye::VulkanRenderer::beginSceneRenderPass(VkCommandBuffer commandBuffer)
{
    assert(isFrameStarted && "Can't call beginSceneRenderPass while frame is not in progress.");
    assert(commandBuffer == getCurrentCommandBuffer() && "Can't begin render pass on command buffer from a different frame.");

    sceneRenderPassInstances[currentFrameIndex].begin(
        commandBuffer,
        makeFullscreenBeginOptions(sceneRenderExtent));
}

void Faye::VulkanRenderer::endSceneRenderPass(VkCommandBuffer commandBuffer)
{
    assert(isFrameStarted && "Can't call endSceneRenderPass while frame is not in progress.");
    assert(commandBuffer == getCurrentCommandBuffer() && "Can't end render pass on command buffer from a different frame.");

    sceneRenderPassInstances[currentFrameIndex].end(commandBuffer);
}

void Faye::VulkanRenderer::beginSwapchainRenderPass(VkCommandBuffer commandBuffer)
{
    assert(isFrameStarted && "Can't call beginSwapchainRenderPass while frame is not in progress.");
    assert(commandBuffer == getCurrentCommandBuffer() && "Can't begin render pass on command buffer from a different frame.");

    vk_swapchain->getRenderPassInstance(currentImageIndex).begin(commandBuffer, makeFullscreenBeginOptions(vk_swapchain->getSwapChainExtent()));
}

void Faye::VulkanRenderer::endSwapchainRenderPass(VkCommandBuffer commandBuffer)
{
    assert(isFrameStarted && "Can't call endSwapchainRenderPass while frame is not in progress.");
    assert(commandBuffer == getCurrentCommandBuffer() && "Can't end render pass on command buffer from a different frame.");

    vk_swapchain->getRenderPassInstance(currentImageIndex).end(commandBuffer);
}

void Faye::VulkanRenderer::beginDepthPrepassRenderPass(VkCommandBuffer commandBuffer)
{
    assert(isFrameStarted && "Can't call beginDepthPrepassRenderPass while frame is not in progress.");
    assert(commandBuffer == getCurrentCommandBuffer() && "Can't begin depth prepass render pass on command buffer from a different frame.");

    depthPrepassRenderPassInstances[currentFrameIndex].begin(
        commandBuffer,
        makeFullscreenBeginOptions(sceneRenderExtent));
}

void Faye::VulkanRenderer::endDepthPrepassRenderPass(VkCommandBuffer commandBuffer)
{
    assert(isFrameStarted && "Can't call endDepthPrepassRenderPass while frame is not in progress.");
    assert(commandBuffer == getCurrentCommandBuffer() && "Can't end depth prepass render pass on command buffer from a different frame.");

    depthPrepassRenderPassInstances[currentFrameIndex].end(commandBuffer);
}

void Faye::VulkanRenderer::beginPostProcessRenderPass(VkCommandBuffer commandBuffer, uint32_t targetIndex)
{
    assert(isFrameStarted && "Can't call beginPostProcessRenderPass while frame is not in progress.");
    assert(commandBuffer == getCurrentCommandBuffer() && "Can't begin render pass on command buffer from a different frame.");
    assert(targetIndex < kPostProcessTargetCount && "Post process target index is out of range.");

    activePostProcessTargetIndex = static_cast<int>(targetIndex);
    postProcessTargets[targetIndex].renderPassInstances[currentFrameIndex].begin(commandBuffer, makeFullscreenBeginOptions(sceneRenderExtent));
}

void Faye::VulkanRenderer::endPostProcessRenderPass(VkCommandBuffer commandBuffer)
{
    assert(isFrameStarted && "Can't call endPostProcessRenderPass while frame is not in progress.");
    assert(commandBuffer == getCurrentCommandBuffer() && "Can't end render pass on command buffer from a different frame.");
    assert(activePostProcessTargetIndex >= 0 && "Can't end post process render pass before it begins.");

    postProcessTargets[static_cast<size_t>(activePostProcessTargetIndex)].renderPassInstances[currentFrameIndex].end(commandBuffer);
    activePostProcessTargetIndex = -1;
}

void Faye::VulkanRenderer::createDepthPrepassRenderPass()
{
    RenderPassBuilder builder{};
    builder
        .setName("Depth Prepass")
        .addDepthAttachment(
            "depthPrepass",
            sceneDepthFormat,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
            VkClearDepthStencilValue{1.0f, 0},
            VK_ATTACHMENT_LOAD_OP_CLEAR,
            VK_ATTACHMENT_STORE_OP_STORE,            // keep depth for sampling
            VK_IMAGE_LAYOUT_UNDEFINED)
        .addSubpass(makeGraphicsSubpass(
            {},                                      // no colour outputs
            std::nullopt,                            // no motion vector output
            makeDepthAttachmentRef("depthPrepass")))
        // Ensure previous fragment-shader reads are done before we write depth.
        .addDependency({VK_SUBPASS_EXTERNAL,
                        0,
                        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                        VK_ACCESS_SHADER_READ_BIT,
                        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                        0})
        // Ensure prepass depth writes are visible to subsequent fragment reads.
        .addDependency({0,
                        VK_SUBPASS_EXTERNAL,
                        VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                        VK_ACCESS_SHADER_READ_BIT,
                        0});

    depthPrepassRenderPass = std::make_unique<VulkanRenderPass>(
        vk_device,
        std::move(builder).build());
}

void Faye::VulkanRenderer::createSceneRenderPass()
{
    RenderPassBuilder builder{};
    builder
        .setName("Scene Offscreen Pass")
        .addColorAttachment(
            "sceneColor",
            sceneColorFormat,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        .addMotionAttachment(
            "sceneMotion",
            sceneMotionFormat,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        .addDepthAttachment(
            "sceneDepth",
            sceneDepthFormat,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL)
        .addSubpass(makeGraphicsSubpass(
            {makeColorAttachmentRef("sceneColor")},
            makeMotionAttachmentRef("sceneMotion"),
            makeDepthAttachmentRef("sceneDepth")))
        .addDependency({VK_SUBPASS_EXTERNAL,
                        0,
                        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                        0,
                        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                        0})
        .addDependency({0,
                        VK_SUBPASS_EXTERNAL,
                        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
                        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                        VK_ACCESS_SHADER_READ_BIT,
                        0});

    sceneRenderPass = std::make_unique<VulkanRenderPass>(
        vk_device,
        std::move(builder).build());
}

void Faye::VulkanRenderer::createPostProcessRenderPass()
{
    RenderPassBuilder builder{};
    builder
        .setName("Post Process Pass")
        .addColorAttachment(
            "postProcessColor",
            sceneColorFormat,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        .addSubpass(makeGraphicsSubpass(
            {makeColorAttachmentRef("postProcessColor")}))
        .addDependency({VK_SUBPASS_EXTERNAL,
                        0,
                        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
                        0,
                        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                        0})
        .addDependency({0,
                        VK_SUBPASS_EXTERNAL,
                        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                        VK_ACCESS_SHADER_READ_BIT,
                        0});

    postProcessRenderPass = std::make_unique<VulkanRenderPass>(
        vk_device,
        std::move(builder).build());
}

void Faye::VulkanRenderer::createSceneRenderTargets()
{
    // Default to swapchain extent only on first creation; thereafter respect panel-driven size.
    if (sceneRenderExtent.width == 0 || sceneRenderExtent.height == 0)
    {
        sceneRenderExtent = vk_swapchain->getSwapChainExtent();
    }
    sceneColorFormat = vk_swapchain->getSwapChainImageFormat();
    sceneMotionFormat = VK_FORMAT_R16G16_SFLOAT;
    sceneDepthFormat = vk_swapchain->findDepthFormat();

    if (sceneRenderPass == nullptr)
    {
        createSceneRenderPass();
    }

    if (depthPrepassRenderPass == nullptr)
    {
        createDepthPrepassRenderPass();
    }

    if (postProcessRenderPass == nullptr)
    {
        createPostProcessRenderPass();
    }

    createSceneImages();
    createPostProcessImages();
    createSceneFramebuffers();
    createPostProcessFramebuffers();
    createSceneViewportSampler();
}

bool Faye::VulkanRenderer::resizeSceneIfNeeded(uint32_t w, uint32_t h)
{
    if (w == 0 || h == 0)
        return false;
    if (sceneRenderExtent.width == w && sceneRenderExtent.height == h)
        return false;

    vkDeviceWaitIdle(vk_device.getDevice());
    sceneRenderExtent = {w, h};
    cleanupSceneRenderTargets();
    cleanupPostProcessRenderTargets();
    createSceneRenderTargets();
    return true;
}

void Faye::VulkanRenderer::createSceneImages()
{
    sceneColorResources.resize(VulkanSwapchain::MAX_FRAMES_IN_FLIGHT);
    sceneMotionResources.resize(VulkanSwapchain::MAX_FRAMES_IN_FLIGHT);
    sceneDepthResources.resize(VulkanSwapchain::MAX_FRAMES_IN_FLIGHT);
    depthPrepassResources.resize(VulkanSwapchain::MAX_FRAMES_IN_FLIGHT);

    for (int i = 0; i < VulkanSwapchain::MAX_FRAMES_IN_FLIGHT; i++)
    {
        // Depth prepass image (opaque-only depth; sampled by water.frag)
        VkImageResourceCreateInfo prepassDepthInfo{};
        prepassDepthInfo.extent = {sceneRenderExtent.width, sceneRenderExtent.height, 1};
        prepassDepthInfo.format = sceneDepthFormat;
        prepassDepthInfo.usage  = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        prepassDepthInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        prepassDepthInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        prepassDepthInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        prepassDepthInfo.memoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        prepassDepthInfo.imageType = VK_IMAGE_TYPE_2D;
        depthPrepassResources[i].createOwned(vk_device, prepassDepthInfo, true);

        VkImageResourceCreateInfo colorCreateInfo{};
        colorCreateInfo.extent = {sceneRenderExtent.width, sceneRenderExtent.height, 1};
        colorCreateInfo.format = sceneColorFormat;
        colorCreateInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        colorCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        colorCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        colorCreateInfo.memoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        colorCreateInfo.imageType = VK_IMAGE_TYPE_2D;

        sceneColorResources[i].createOwned(vk_device, colorCreateInfo, true);

        VkImageResourceCreateInfo depthCreateInfo{};
        depthCreateInfo.extent = {sceneRenderExtent.width, sceneRenderExtent.height, 1};
        depthCreateInfo.format = sceneDepthFormat;
        depthCreateInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        depthCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        depthCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        depthCreateInfo.memoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        depthCreateInfo.imageType = VK_IMAGE_TYPE_2D;

        sceneDepthResources[i].createOwned(vk_device, depthCreateInfo, true);

        VkImageResourceCreateInfo motionCreateInfo{};
        motionCreateInfo.extent = {sceneRenderExtent.width, sceneRenderExtent.height, 1};
        motionCreateInfo.format = sceneMotionFormat;
        motionCreateInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        motionCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        motionCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        motionCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        motionCreateInfo.memoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        motionCreateInfo.imageType = VK_IMAGE_TYPE_2D;

        sceneMotionResources[i].createOwned(vk_device, motionCreateInfo, true);
    }
}

void Faye::VulkanRenderer::createPostProcessImages()
{
    for (auto &target : postProcessTargets)
    {
        target.images.resize(VulkanSwapchain::MAX_FRAMES_IN_FLIGHT);
        target.imageMemories.resize(VulkanSwapchain::MAX_FRAMES_IN_FLIGHT);
        target.imageViews.resize(VulkanSwapchain::MAX_FRAMES_IN_FLIGHT);

        for (int i = 0; i < VulkanSwapchain::MAX_FRAMES_IN_FLIGHT; i++)
        {
            VkImageCreateInfo postProcessColorImageInfo{};
            postProcessColorImageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            postProcessColorImageInfo.imageType = VK_IMAGE_TYPE_2D;
            postProcessColorImageInfo.extent.width = sceneRenderExtent.width;
            postProcessColorImageInfo.extent.height = sceneRenderExtent.height;
            postProcessColorImageInfo.extent.depth = 1;
            postProcessColorImageInfo.mipLevels = 1;
            postProcessColorImageInfo.arrayLayers = 1;
            postProcessColorImageInfo.format = sceneColorFormat;
            postProcessColorImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
            postProcessColorImageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            postProcessColorImageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
            postProcessColorImageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
            postProcessColorImageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

            vk_device.createImageWithInfo(
                postProcessColorImageInfo,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                target.images[i],
                target.imageMemories[i]);
            target.imageViews[i] = createImageView(target.images[i], sceneColorFormat, VK_IMAGE_ASPECT_COLOR_BIT);
        }
    }
}

void Faye::VulkanRenderer::createSceneFramebuffers()
{
    depthPrepassRenderPassInstances.clear();
    depthPrepassRenderPassInstances.reserve(VulkanSwapchain::MAX_FRAMES_IN_FLIGHT);
    for (int i = 0; i < VulkanSwapchain::MAX_FRAMES_IN_FLIGHT; i++)
    {
        depthPrepassRenderPassInstances.emplace_back(
            vk_device,
            *depthPrepassRenderPass,
            sceneRenderExtent,
            std::vector<VkImageView>{depthPrepassResources[i].imageView});
    }

    sceneRenderPassInstances.clear();
    sceneRenderPassInstances.reserve(VulkanSwapchain::MAX_FRAMES_IN_FLIGHT);

    for (int i = 0; i < VulkanSwapchain::MAX_FRAMES_IN_FLIGHT; i++)
    {
        sceneRenderPassInstances.emplace_back(
            vk_device,
            *sceneRenderPass,
            sceneRenderExtent,
            std::vector<VkImageView>{sceneColorResources[i].imageView, sceneMotionResources[i].imageView, sceneDepthResources[i].imageView});
    }
}

void Faye::VulkanRenderer::createPostProcessFramebuffers()
{
    for (auto &target : postProcessTargets)
    {
        target.renderPassInstances.clear();
        target.renderPassInstances.reserve(VulkanSwapchain::MAX_FRAMES_IN_FLIGHT);

        for (int i = 0; i < VulkanSwapchain::MAX_FRAMES_IN_FLIGHT; i++)
        {
            target.renderPassInstances.emplace_back(
                vk_device,
                *postProcessRenderPass,
                sceneRenderExtent,
                std::vector<VkImageView>{target.imageViews[i]});
        }
    }
}

void Faye::VulkanRenderer::cleanupSceneRenderTargets()
{
    depthPrepassRenderPassInstances.clear();
    sceneRenderPassInstances.clear();

    for (size_t i = 0; i < depthPrepassResources.size(); i++)
    {
        depthPrepassResources[i].destroy(vk_device.getDevice());
    }
    depthPrepassResources.clear();

    for (size_t i = 0; i < sceneColorResources.size(); i++)
    {
        sceneColorResources[i].destroy(vk_device.getDevice());
    }

    for (size_t i = 0; i < sceneMotionResources.size(); i++)
    {
        sceneMotionResources[i].destroy(vk_device.getDevice());
    }

    for (size_t i = 0; i < sceneDepthResources.size(); i++)
    {
        sceneDepthResources[i].destroy(vk_device.getDevice());
    }

    sceneColorResources.clear();
    sceneMotionResources.clear();
    sceneDepthResources.clear();
}

void Faye::VulkanRenderer::cleanupPostProcessRenderTargets()
{
    for (auto &target : postProcessTargets)
    {
        target.renderPassInstances.clear();

        for (size_t i = 0; i < target.imageViews.size(); i++)
        {
            vkDestroyImageView(vk_device.getDevice(), target.imageViews[i], nullptr);
            vkDestroyImage(vk_device.getDevice(), target.images[i], nullptr);
            vkFreeMemory(vk_device.getDevice(), target.imageMemories[i], nullptr);
        }

        target.images.clear();
        target.imageMemories.clear();
        target.imageViews.clear();
    }
}

void Faye::VulkanRenderer::createSceneViewportSampler()
{
    if (sceneViewportSampler != VK_NULL_HANDLE)
    {
        return;
    }

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;
    samplerInfo.mipLodBias = 0.0f;

    if (vkCreateSampler(vk_device.getDevice(), &samplerInfo, nullptr, &sceneViewportSampler) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create scene viewport sampler");
    }
}

void Faye::VulkanRenderer::destroySceneViewportSampler()
{
    if (sceneViewportSampler != VK_NULL_HANDLE)
    {
        vkDestroySampler(vk_device.getDevice(), sceneViewportSampler, nullptr);
        sceneViewportSampler = VK_NULL_HANDLE;
    }
}


void Faye::VulkanRenderer::destroyDepthPrepassRenderPass()
{
    depthPrepassRenderPassInstances.clear();
    depthPrepassRenderPass.reset();
}

void Faye::VulkanRenderer::destroySceneRenderPass()
{
    sceneRenderPass.reset();
}

void Faye::VulkanRenderer::destroyPostProcessRenderPass()
{
    postProcessRenderPass.reset();
}

VkImageView Faye::VulkanRenderer::createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags)
{
    VkImageViewCreateInfo viewInfo = {};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = aspectFlags;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    VkImageView imageView = VK_NULL_HANDLE;
    if (vkCreateImageView(vk_device.getDevice(), &viewInfo, nullptr, &imageView) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create scene image view");
    }

    return imageView;
}


// void Faye::VulkanRenderer::createDescriptorSetLayout()
// {
//     VkDescriptorSetLayoutBinding uboLayoutBinding = {};
//     uboLayoutBinding.binding = 0;
//     uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
//     uboLayoutBinding.descriptorCount = 1;

//     uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
//     uboLayoutBinding.pImmutableSamplers = nullptr;

//     VkDescriptorSetLayoutBinding samplerLayoutBinding = {};
//     samplerLayoutBinding.binding = 1;
//     samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
//     samplerLayoutBinding.descriptorCount = 1;
//     samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
//     samplerLayoutBinding.pImmutableSamplers = nullptr;

//     std::array<VkDescriptorSetLayoutBinding, 2> bindings = {uboLayoutBinding, samplerLayoutBinding};
//     VkDescriptorSetLayoutCreateInfo layoutInfo = {};
//     layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
//     layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
//     layoutInfo.pBindings = bindings.data();

//     if (vkCreateDescriptorSetLayout(vk_device.getDevice(), &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS)
//     {
//         throw std::runtime_error("Failed to create descriptor set layout");
//     }

// }

void Faye::VulkanRenderer::freeCommandBuffers()
{
    if (commandBuffers.empty())
    {
        return;
    }

    vkFreeCommandBuffers(vk_device.getDevice(), *vk_device.getCommandPool(), static_cast<uint32_t>(commandBuffers.size()), commandBuffers.data());
    commandBuffers.clear();
}

// void Faye::VulkanRenderer::generateMipmaps(VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels)
// {
//     // Check if image format supports linear blitting
//     VkFormatProperties formatProperties;
//     vkGetPhysicalDeviceFormatProperties(vk_device.getPhysicalDevice(), imageFormat, &formatProperties);

//     if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT))
//     {
//         throw std::runtime_error("Texture image format does not support linear blitting!");
//     }

//     VkCommandBuffer commandBuffer = vk_device.beginSingleTimeCommands();

//     VkImageMemoryBarrier barrier = {};
//     barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
//     barrier.image = image;
//     barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
//     barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
//     barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
//     barrier.subresourceRange.baseArrayLayer = 0;
//     barrier.subresourceRange.layerCount = 1;
//     barrier.subresourceRange.levelCount = 1;

//     int32_t mipWidth = texWidth;
//     int32_t mipHeight = texHeight;

//     for (uint32_t i = 1; i < mipLevels; i++)
//     {
//         barrier.subresourceRange.baseMipLevel = i - 1;
//         barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
//         barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
//         barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
//         barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

//         vkCmdPipelineBarrier(commandBuffer,
//                              VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
//                              0, nullptr,
//                              0, nullptr,
//                              1, &barrier);

//         VkImageBlit blit{};
//         blit.srcOffsets[0] = {0, 0, 0};
//         blit.srcOffsets[1] = {mipWidth, mipHeight, 1};
//         blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
//         blit.srcSubresource.mipLevel = i - 1;
//         blit.srcSubresource.baseArrayLayer = 0;
//         blit.srcSubresource.layerCount = 1;
//         blit.dstOffsets[0] = {0, 0, 0};
//         blit.dstOffsets[1] = {mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1};
//         blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
//         blit.dstSubresource.mipLevel = i;
//         blit.dstSubresource.baseArrayLayer = 0;
//         blit.dstSubresource.layerCount = 1;

//         vkCmdBlitImage(commandBuffer,
//                        image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
//                        image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
//                        1, &blit,
//                        VK_FILTER_LINEAR);

//         barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
//         barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
//         barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
//         barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

//         vkCmdPipelineBarrier(commandBuffer,
//                              VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
//                              0, nullptr,
//                              0, nullptr,
//                              1, &barrier);

//         if (mipWidth > 1)
//             mipWidth /= 2;
//         if (mipHeight > 1)
//             mipHeight /= 2;
//     }

//     barrier.subresourceRange.baseMipLevel = mipLevels - 1;
//     barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
//     barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
//     barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
//     barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

//     vkCmdPipelineBarrier(commandBuffer,
//                          VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
//                          0, nullptr,
//                          0, nullptr,
//                          1, &barrier);

//     vk_device.endSingleTimeCommands(commandBuffer);
// }

// bool Faye::VulkanRenderer::hasStencilComponent(VkFormat format)
// {
//     return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
// }

// void Faye::VulkanRenderer::createTextureImageView()
// {
//     textureImageView = createImageView(textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT, mipLevels);
// }

// VkImageView Faye::VulkanRenderer::createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels)
// {
//     VkImageViewCreateInfo viewInfo = {};
//     viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
//     viewInfo.image = image;
//     viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
//     viewInfo.format = format;
//     viewInfo.subresourceRange.aspectMask = aspectFlags;
//     viewInfo.subresourceRange.baseMipLevel = 0;
//     viewInfo.subresourceRange.levelCount = mipLevels;
//     viewInfo.subresourceRange.baseArrayLayer = 0;
//     viewInfo.subresourceRange.layerCount = 1;

//     VkImageView imageView;
//     if (vkCreateImageView(vk_device.getDevice(), &viewInfo, nullptr, &imageView) != VK_SUCCESS)
//     {
//         throw std::runtime_error("Failed to create texture image view");
//     }

//     return imageView;
// }

// void Faye::VulkanRenderer::transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipLevels)
// {
//     VkCommandBuffer commandBuffer = vk_device.beginSingleTimeCommands();

//     VkImageMemoryBarrier barrier{};
//     barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
//     barrier.oldLayout = oldLayout;
//     barrier.newLayout = newLayout;
//     barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
//     barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
//     barrier.image = image;
//     barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
//     barrier.subresourceRange.baseMipLevel = 0;
//     barrier.subresourceRange.levelCount = mipLevels;
//     barrier.subresourceRange.baseArrayLayer = 0;
//     barrier.subresourceRange.layerCount = 1;

//     VkPipelineStageFlags sourceStage;
//     VkPipelineStageFlags destinationStage;

//     if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
//     {
//         barrier.srcAccessMask = 0;
//         barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

//         sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
//         destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
//     }
//     else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
//     {
//         barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
//         barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

//         sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
//         destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
//     }
//     else
//     {
//         throw std::invalid_argument("unsupported layout transition!");
//     }

//     vkCmdPipelineBarrier(
//         commandBuffer,
//         sourceStage, destinationStage,
//         0,
//         0, nullptr,
//         0, nullptr,
//         1, &barrier);

//     vk_device.endSingleTimeCommands(commandBuffer);
// }

// void Faye::VulkanRenderer::createTextureSampler()
// {
//     VkPhysicalDeviceProperties properties = {};
//     vkGetPhysicalDeviceProperties(vk_device.getPhysicalDevice(), &properties);

//     VkSamplerCreateInfo samplerInfo = {};
//     samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
//     samplerInfo.magFilter = VK_FILTER_LINEAR;
//     samplerInfo.minFilter = VK_FILTER_LINEAR;
//     samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
//     samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
//     samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
//     samplerInfo.anisotropyEnable = VK_TRUE;
//     samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
//     samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
//     samplerInfo.unnormalizedCoordinates = VK_FALSE;
//     samplerInfo.compareEnable = VK_FALSE;
//     samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
//     samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
//     samplerInfo.mipLodBias = 0.0f;
//     samplerInfo.minLod = 0.0f;
//     samplerInfo.maxLod = static_cast<float>(mipLevels);

//     if (vkCreateSampler(vk_device.getDevice(), &samplerInfo, nullptr, &textureSampler) != VK_SUCCESS)
//     {
//         throw std::runtime_error("Failed to create texture sampler!");
//     }
// }

// void Faye::VulkanRenderer::createImage(uint32_t width, uint32_t height, uint32_t mipLevels, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage &image, VkDeviceMemory &imageMemory)
// {
//     VkImageCreateInfo imageInfo{};
//     imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
//     imageInfo.imageType = VK_IMAGE_TYPE_2D;
//     imageInfo.extent.width = static_cast<uint32_t>(width);
//     imageInfo.extent.height = static_cast<uint32_t>(height);
//     imageInfo.extent.depth = 1;
//     imageInfo.mipLevels = mipLevels;
//     imageInfo.arrayLayers = 1;
//     imageInfo.format = format;
//     imageInfo.tiling = tiling;
//     imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
//     imageInfo.usage = usage;
//     imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
//     imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

//     if (vkCreateImage(vk_device.getDevice(), &imageInfo, nullptr, &image) != VK_SUCCESS)
//     {
//         throw std::runtime_error("failed to create image!");
//     }

//     VkMemoryRequirements memRequirements;
//     vkGetImageMemoryRequirements(vk_device.getDevice(), image, &memRequirements);

//     VkMemoryAllocateInfo allocInfo{};
//     allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
//     allocInfo.allocationSize = memRequirements.size;
//     allocInfo.memoryTypeIndex = vk_device.findMemoryType(memRequirements.memoryTypeBits, properties);

//     if (vkAllocateMemory(vk_device.getDevice(), &allocInfo, nullptr, &imageMemory) != VK_SUCCESS)
//     {
//         throw std::runtime_error("failed to allocate image memory!");
//     }

//     vkBindImageMemory(vk_device.getDevice(), image, imageMemory, 0);
// }

// void Faye::VulkanRenderer::updateUniformBuffer(uint32_t currentImage, Faye::Camera &camera)
// {
//     static auto startTime = std::chrono::high_resolution_clock::now();

//     auto currentTime = std::chrono::high_resolution_clock::now();
//     float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

//     UniformBufferObject ubo{};

//     ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(45.0f), glm::vec3(0.0f, 0.0f, 1.0f));
//     ubo.view = camera.getViewMatrix(); //glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
//     ubo.proj = glm::perspective(glm::radians(80.0f), vk_swapchain->getSwapChainExtent().width / (float)vk_swapchain->getSwapChainExtent().height, 0.1f, 100.0f);
//     ubo.proj[1][1] *= -1;

//     memcpy(uniformBuffersMapped[currentImage], &ubo, sizeof(ubo));
// }

// ------------------------------------- Conversion Functions ------------------------------------- //

void Faye::VulkanRenderer::recreateSwapchain()
{
    auto extent = window.getExtent();

    while (extent.width == 0 || extent.height == 0)
    {
        glfwWaitEventsTimeout(0.1);

        if (window.shouldClose())
        {
            return;
        }

        extent = window.getExtent();
    }

    vkDeviceWaitIdle(vk_device.getDevice());

    if (vk_swapchain == nullptr)
    {
        vk_swapchain = std::make_unique<VulkanSwapchain>(&vk_device, extent);
    }
    else
    {
        std::shared_ptr<VulkanSwapchain> oldSwapchain = std::move(vk_swapchain);
        vk_swapchain = std::make_unique<VulkanSwapchain>(&vk_device, extent, oldSwapchain);

        if (!oldSwapchain->compareSwapFormats(*vk_swapchain.get()))
        {
            throw std::runtime_error("Swap chain image or depth format has changed!");
        }
    }

    ++swapchainGeneration;

    if (sceneRenderPassInstances.empty())
    {
        createSceneRenderTargets();
    }
}

void Faye::VulkanRenderer::createCommandBuffers()
{
    commandBuffers.resize(VulkanSwapchain::MAX_FRAMES_IN_FLIGHT);
    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = *vk_device.getCommandPool();
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());

    if (vkAllocateCommandBuffers(vk_device.getDevice(), &allocInfo, commandBuffers.data()) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to allocate command buffers!");
    }
}
