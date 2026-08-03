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

Faye::VulkanRenderer::VulkanRenderer(Window &win, VulkanDevice &device, Profiler::VkProfiler &profiler) : window{win}, vk_device{device}, profiler{profiler}
{
    LOG_INFO(Logger::get(), "Creating Vulkan Device class instance...");

    LOG_INFO(Logger::get(), "Creating Vulkan Swapchain...");
    recreateSwapchain();
    LOG_INFO(Logger::get(), "Creating Vulkan CommandBuffers...");
    createCommandBuffers();
}

Faye::VulkanRenderer::~VulkanRenderer()
{
    cleanupSceneRenderTargets();
    cleanupPostProcessRenderTargets();
    destroySceneViewportSampler();
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

Faye::RenderBackendHandles Faye::VulkanRenderer::getBackendHandles() const
{
    RenderBackendHandles handles{};
    handles.instance = vk_device.getInstance();
    handles.physicalDevice = vk_device.getPhysicalDevice();
    handles.device = vk_device.getDevice();
    handles.queueFamily = vk_device.getGraphicsQueueFamilyIndex();
    handles.queue = vk_device.getGraphicsQueue();
    handles.swapchainColorFormat = getSwapchainColorFormat();
    handles.minImageCount = VulkanSwapchain::MAX_FRAMES_IN_FLIGHT;
    handles.imageCount = VulkanSwapchain::MAX_FRAMES_IN_FLIGHT;
    return handles;
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
    
    // resets the query pool/profiler frame data for the frame
    profiler.beginFrame(commandBuffer, currentFrameIndex);
    profiler.beginScope(commandBuffer, "Frame");

    return commandBuffer;
}

void Faye::VulkanRenderer::endFrame()
{
    assert(isFrameStarted && "Can't call endFrame while frame is not in progress.");
    auto commandBuffer = getCurrentCommandBuffer();

    profiler.endScope(commandBuffer); // end the "Frame" scope
    profiler.endFrame(commandBuffer, currentFrameIndex);

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

    auto &color = sceneColorResources[currentFrameIndex];
    auto &motion = sceneMotionResources[currentFrameIndex];
    auto &depth = sceneDepthResources[currentFrameIndex];

    color.recordTransition(commandBuffer, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                           VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                           VK_ACCESS_2_SHADER_READ_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);
    motion.recordTransition(commandBuffer, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                            VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                            VK_ACCESS_2_SHADER_READ_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);
    depth.recordTransition(commandBuffer, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                           VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
                           VK_ACCESS_2_SHADER_READ_BIT, VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);

    VkRenderingAttachmentInfo colorAtt{};
    colorAtt.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAtt.imageView = color.imageView;
    colorAtt.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAtt.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAtt.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAtt.clearValue.color = {{0, 0, 0, 1}};

    VkRenderingAttachmentInfo motionAtt = colorAtt; // same load/store/clear
    motionAtt.imageView = motion.imageView;

    VkRenderingAttachmentInfo colorAtts[2] = {colorAtt, motionAtt}; // order matches pipeline formats

    VkRenderingAttachmentInfo depthAtt{};
    depthAtt.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    depthAtt.imageView = depth.imageView;
    depthAtt.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    depthAtt.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAtt.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depthAtt.clearValue.depthStencil = {1.0f, 0};

    VkRenderingInfo info{};
    info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    info.renderArea = {{0, 0}, sceneRenderExtent};
    info.layerCount = 1;
    info.colorAttachmentCount = 2;
    info.pColorAttachments = colorAtts;
    info.pDepthAttachment = &depthAtt;
    vkCmdBeginRendering(commandBuffer, &info);
    // + vkCmdSetViewport / vkCmdSetScissor as in 3.1

    VkViewport vp{0, 0, (float)sceneRenderExtent.width, (float)sceneRenderExtent.height, 0, 1};
    VkRect2D sc{{0, 0}, sceneRenderExtent};
    vkCmdSetViewport(commandBuffer, 0, 1, &vp);
    vkCmdSetScissor(commandBuffer, 0, 1, &sc);
}

void Faye::VulkanRenderer::endSceneRenderPass(VkCommandBuffer commandBuffer)
{
    assert(isFrameStarted && "Can't call endSceneRenderPass while frame is not in progress.");
    assert(commandBuffer == getCurrentCommandBuffer() && "Can't end render pass on command buffer from a different frame.");

    vkCmdEndRendering(commandBuffer);

    sceneColorResources[currentFrameIndex].recordTransition(commandBuffer,
                                                            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                            VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                                                            VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_2_SHADER_READ_BIT);
    sceneMotionResources[currentFrameIndex].recordTransition(commandBuffer,
                                                             VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                                             VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                                                             VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_2_SHADER_READ_BIT);
    sceneDepthResources[currentFrameIndex].recordTransition(commandBuffer,
                                                            VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL,
                                                            VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                                                            VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_ACCESS_2_SHADER_READ_BIT);
}

void Faye::VulkanRenderer::beginSwapchainRenderPass(VkCommandBuffer commandBuffer)
{
    assert(isFrameStarted && "Can't call beginSwapchainRenderPass while frame is not in progress.");
    assert(commandBuffer == getCurrentCommandBuffer() && "Can't begin render pass on command buffer from a different frame.");

    VkImage img = vk_swapchain->getImage(currentImageIndex); // add getter if needed
    VkImageView view = vk_swapchain->getImageView(currentImageIndex);

    // Acquire->color: the image-available semaphore gates the QUEUE; this gates LAYOUT.
    VkImageResource::imageBarrier(commandBuffer, img, VK_IMAGE_ASPECT_COLOR_BIT,
                                  VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                  VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                  0, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);

    VkRenderingAttachmentInfo colorAtt{};
    colorAtt.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAtt.imageView = view;
    colorAtt.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAtt.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAtt.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAtt.clearValue.color = {{0, 0, 0, 1}};

    VkRenderingInfo info{};
    info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    info.renderArea = {{0, 0}, vk_swapchain->getSwapChainExtent()};
    info.layerCount = 1;
    info.colorAttachmentCount = 1;
    info.pColorAttachments = &colorAtt;
    info.pDepthAttachment = nullptr; // see depth note below
    vkCmdBeginRendering(commandBuffer, &info);

    const VkExtent2D swapExtent = vk_swapchain->getSwapChainExtent();
    VkViewport vp{0, 0, (float)swapExtent.width, (float)swapExtent.height, 0, 1};
    VkRect2D sc{{0, 0}, swapExtent};
    vkCmdSetViewport(commandBuffer, 0, 1, &vp);
    vkCmdSetScissor(commandBuffer, 0, 1, &sc);
}

void Faye::VulkanRenderer::endSwapchainRenderPass(VkCommandBuffer commandBuffer)
{
    assert(isFrameStarted && "Can't call endSwapchainRenderPass while frame is not in progress.");
    assert(commandBuffer == getCurrentCommandBuffer() && "Can't end render pass on command buffer from a different frame.");

    vkCmdEndRendering(commandBuffer);

    VkImageResource::imageBarrier(commandBuffer, vk_swapchain->getImage(currentImageIndex), VK_IMAGE_ASPECT_COLOR_BIT,
                                  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                                  VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT,
                                  VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, 0);
}

void Faye::VulkanRenderer::beginDepthPrepassRenderPass(VkCommandBuffer commandBuffer)
{
    assert(isFrameStarted && "Can't call beginDepthPrepassRenderPass while frame is not in progress.");
    assert(commandBuffer == getCurrentCommandBuffer() && "Can't begin depth prepass render pass on command buffer from a different frame.");

    auto &depth = depthPrepassResources[currentFrameIndex];
    // Re-add the EXTERNAL->0 dependency (vk_renderer.cpp:242): prior frame's shader
    // reads of this image must finish before we write depth again.
    depth.recordTransition(commandBuffer,
                           VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                           VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
                           VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                           VK_ACCESS_2_SHADER_READ_BIT,
                           VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);

    VkRenderingAttachmentInfo depthAtt{};
    depthAtt.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    depthAtt.imageView = depth.imageView;
    depthAtt.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    depthAtt.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;   // was the pass's loadOp
    depthAtt.storeOp = VK_ATTACHMENT_STORE_OP_STORE; // keep for sampling
    depthAtt.clearValue.depthStencil = {1.0f, 0};

    VkRenderingInfo info{};
    info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    info.renderArea = {{0, 0}, sceneRenderExtent};
    info.layerCount = 1;
    info.colorAttachmentCount = 0; // depth-only
    info.pColorAttachments = nullptr;
    info.pDepthAttachment = &depthAtt;
    vkCmdBeginRendering(commandBuffer, &info);

    VkViewport vp{0, 0, (float)sceneRenderExtent.width, (float)sceneRenderExtent.height, 0, 1};
    VkRect2D sc{{0, 0}, sceneRenderExtent};
    vkCmdSetViewport(commandBuffer, 0, 1, &vp);
    vkCmdSetScissor(commandBuffer, 0, 1, &sc);
}

void Faye::VulkanRenderer::endDepthPrepassRenderPass(VkCommandBuffer commandBuffer)
{
    assert(isFrameStarted && "Can't call endDepthPrepassRenderPass while frame is not in progress.");
    assert(commandBuffer == getCurrentCommandBuffer() && "Can't end depth prepass render pass on command buffer from a different frame.");

    vkCmdEndRendering(commandBuffer);

    depthPrepassResources[currentFrameIndex].recordTransition(commandBuffer,
                                                              VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL,
                                                              VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
                                                              VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                                                              VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
                                                              VK_ACCESS_2_SHADER_READ_BIT);
}

void Faye::VulkanRenderer::beginPostProcessRenderPass(VkCommandBuffer commandBuffer, uint32_t targetIndex)
{
    assert(isFrameStarted && "Can't call beginPostProcessRenderPass while frame is not in progress.");
    assert(commandBuffer == getCurrentCommandBuffer() && "Can't begin render pass on command buffer from a different frame.");
    assert(targetIndex < kPostProcessTargetCount && "Post process target index is out of range.");

    activePostProcessTargetIndex = static_cast<int>(targetIndex);

    auto &target = postProcessTargets[targetIndex];

    // Contents are fully overwritten each frame (loadOp CLEAR), so discarding via
    // UNDEFINED is fine and avoids tracking the image's previous layout.
    VkImageResource::imageBarrier(commandBuffer, target.images[currentFrameIndex], VK_IMAGE_ASPECT_COLOR_BIT,
                                  VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                  VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                  VK_ACCESS_2_SHADER_READ_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);

    VkRenderingAttachmentInfo colorAtt{};
    colorAtt.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAtt.imageView = target.imageViews[currentFrameIndex];
    colorAtt.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAtt.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAtt.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAtt.clearValue.color = {{0, 0, 0, 1}};

    VkRenderingInfo info{};
    info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    info.renderArea = {{0, 0}, sceneRenderExtent};
    info.layerCount = 1;
    info.colorAttachmentCount = 1;
    info.pColorAttachments = &colorAtt;
    vkCmdBeginRendering(commandBuffer, &info);

    VkViewport vp{0, 0, (float)sceneRenderExtent.width, (float)sceneRenderExtent.height, 0, 1};
    VkRect2D sc{{0, 0}, sceneRenderExtent};
    vkCmdSetViewport(commandBuffer, 0, 1, &vp);
    vkCmdSetScissor(commandBuffer, 0, 1, &sc);
}

void Faye::VulkanRenderer::endPostProcessRenderPass(VkCommandBuffer commandBuffer)
{
    assert(isFrameStarted && "Can't call endPostProcessRenderPass while frame is not in progress.");
    assert(commandBuffer == getCurrentCommandBuffer() && "Can't end render pass on command buffer from a different frame.");
    assert(activePostProcessTargetIndex >= 0 && "Can't end post process render pass before it begins.");

    vkCmdEndRendering(commandBuffer);

    VkImageResource::imageBarrier(commandBuffer,
                                  postProcessTargets[static_cast<size_t>(activePostProcessTargetIndex)].images[currentFrameIndex],
                                  VK_IMAGE_ASPECT_COLOR_BIT,
                                  VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                  VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                                  VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_2_SHADER_READ_BIT);

    activePostProcessTargetIndex = -1;
}

void Faye::VulkanRenderer::createSceneRenderTargets()
{
    // Default to swapchain extent only on first creation; thereafter respect panel-driven size.
    if (sceneRenderExtent.width == 0 || sceneRenderExtent.height == 0)
    {
        sceneRenderExtent = vk_swapchain->getSwapChainExtent();
    }
    sceneColorFormat = VK_FORMAT_R16G16B16A16_SFLOAT;// vk_swapchain->getSwapChainImageFormat();
    sceneMotionFormat = VK_FORMAT_R16G16_SFLOAT;
    sceneDepthFormat = vk_swapchain->findDepthFormat();

    createSceneImages();
    createPostProcessImages();
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
        prepassDepthInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        prepassDepthInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        prepassDepthInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        prepassDepthInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        prepassDepthInfo.memoryProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        prepassDepthInfo.imageType = VK_IMAGE_TYPE_2D;
        prepassDepthInfo.debugName = "Depth Prepass Image";
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
        colorCreateInfo.debugName = "Scene Color Image";
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
        depthCreateInfo.debugName = "Scene Depth Image";
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
        motionCreateInfo.debugName = "Scene Motion Image";
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

            VmaAllocationCreateInfo allocInfo{};
            allocInfo.usage = VMA_MEMORY_USAGE_AUTO;

            vk_device.createImageWithInfo(
                postProcessColorImageInfo,
                allocInfo,
                target.images[i],
                target.imageMemories[i]);
            target.imageViews[i] = createImageView(target.images[i], sceneColorFormat, VK_IMAGE_ASPECT_COLOR_BIT);
        }
    }
}

void Faye::VulkanRenderer::cleanupSceneRenderTargets()
{
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
        for (size_t i = 0; i < target.imageViews.size(); i++)
        {
            vkDestroyImageView(vk_device.getDevice(), target.imageViews[i], nullptr);
            vmaDestroyImage(vk_device.getAllocator(), target.images[i], target.imageMemories[i]);
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

void Faye::VulkanRenderer::freeCommandBuffers()
{
    if (commandBuffers.empty())
    {
        return;
    }

    vkFreeCommandBuffers(vk_device.getDevice(), *vk_device.getCommandPool(), static_cast<uint32_t>(commandBuffers.size()), commandBuffers.data());
    commandBuffers.clear();
}

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

    if (sceneColorResources.empty())
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
