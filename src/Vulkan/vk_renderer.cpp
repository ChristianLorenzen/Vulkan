#define VK_USER_PLATFORM_MACOS_MVK
#include <vulkan/vulkan.h>
#include <stdio.h>
#include <unordered_map>

// #define STB_IMAGE_IMPLEMENTATION
// #include "include/stb_image.h"

//#define TINYOBJLOADER_IMPLEMENTATION
//#include "include/tiny_obj_loader.h"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_vulkan.h"

#include "Vulkan.hpp"
#include "Window/Window.hpp"
#include "Structures/Model.hpp"
#include "Structures/Vertex.hpp"
#include "Structures/UBO.hpp"

#include "quill/LogMacros.h"


#include "GUIComponents/imgui_Components.hpp"

#include <chrono>

#include "Camera/Camera.hpp"
#include "vk_renderer.hpp"
using namespace Faye;

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

const std::string MODEL_PATH = "src/include/viking_room.obj";
const std::string TEXTURE_PATH = "src/include/viking_room.png";


Faye::VulkanRenderer::VulkanRenderer(Window &win, VulkanDevice &device) : window{win}, vk_device{device}
{
    LOG_INFO(Logger::getInstance(), "Creating Vulkan Device class instance...");

    LOG_INFO(Logger::getInstance(), "Creating Vulkan Swapchain...");
    recreateSwapchain();
    LOG_INFO(Logger::getInstance(), "Creating Vulkan CommandBuffers...");
    createCommandBuffers();

}

Faye::VulkanRenderer::~VulkanRenderer() {
 freeCommandBuffers();
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
    else if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to present swap chain image");
    }

    isFrameStarted = false;
    currentFrameIndex = (currentFrameIndex + 1) % VulkanSwapchain::MAX_FRAMES_IN_FLIGHT;
}

void Faye::VulkanRenderer::beginSwapchainRenderPass(VkCommandBuffer commandBuffer)
{
    assert(isFrameStarted && "Can't call beginSwapchainRenderPass while frame is not in progress.");
    assert(commandBuffer == getCurrentCommandBuffer() && "Can't begin render pass on command buffer from a different frame.");

    VkRenderPassBeginInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = vk_swapchain->getRenderPass();
    renderPassInfo.framebuffer = vk_swapchain->getFrameBuffer(currentImageIndex);
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = vk_swapchain->getSwapChainExtent();

    std::array<VkClearValue, 2> clearValues = {};
    // TODO: Create variable to change background color
    clearValues[0].color = {0.0f, 0.0f, 0.0f, 1.0f};
    clearValues[1].depthStencil = {1.0f, 0};

    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport = {};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)vk_swapchain->getSwapChainExtent().width;
    viewport.height = (float)vk_swapchain->getSwapChainExtent().height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor = {};
    scissor.offset = {0, 0};
    scissor.extent = vk_swapchain->getSwapChainExtent();
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
}

void Faye::VulkanRenderer::endSwapchainRenderPass(VkCommandBuffer commandBuffer)
{
    assert(isFrameStarted && "Can't call endSwapchainRenderPass while frame is not in progress.");
    assert(commandBuffer == getCurrentCommandBuffer() && "Can't end render pass on command buffer from a different frame.");

    vkCmdEndRenderPass(commandBuffer);
}

void Faye::VulkanRenderer::initImGui(VkDescriptorPool descriptorPool)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    // io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

    // Add Roboto as default font.
    io.FontDefault = io.Fonts->AddFontFromFileTTF("src/include/fonts/Poppins,Roboto/Roboto/Roboto-Regular.ttf", 16.0f);

    // Add custom styling.
    ImGuiStyle &style = ImGui::GetStyle();
    style.FrameRounding = 2.0f;
    style.WindowRounding = 4.f;
    style.WindowBorderSize = 0.0f;

    ImGui::StyleColorsCustom();

    // ImGuiStyle& style = ImGui::GetStyle();
    // if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
    //     style.WindowRounding = 0.0f;
    //     style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    // }

    ImGui_ImplGlfw_InitForVulkan(window.getWindow(), true);
    ImGui_ImplVulkan_InitInfo info = {};
    info.Instance = vk_device.getInstance();
    info.PhysicalDevice = vk_device.getPhysicalDevice();
    info.Device = vk_device.getDevice();
    info.QueueFamily = 0;
    info.Queue = vk_device.getGraphicsQueue();
    info.DescriptorPool = descriptorPool;
    info.RenderPass = vk_swapchain->getRenderPass();

    info.MinImageCount = VulkanSwapchain::MAX_FRAMES_IN_FLIGHT;
    info.ImageCount = VulkanSwapchain::MAX_FRAMES_IN_FLIGHT;

    ImGui_ImplVulkan_Init(&info);

    VkCommandBuffer commandBuffer = vk_device.beginSingleTimeCommands();

    ImGui_ImplVulkan_CreateFontsTexture();

    vk_device.endSingleTimeCommands(commandBuffer);

    vkDeviceWaitIdle(vk_device.getDevice());
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
        extent = window.getExtent();
        glfwWaitEvents();
    }

    vkDeviceWaitIdle(vk_device.getDevice());

    if (vk_swapchain == nullptr)
    {
        vk_swapchain = std::make_unique<VulkanSwapchain>(&vk_device, extent);
    } else {
        std::shared_ptr<VulkanSwapchain> oldSwapchain = std::move(vk_swapchain);
        vk_swapchain = std::make_unique<VulkanSwapchain>(&vk_device, extent, oldSwapchain);

        if (!oldSwapchain->compareSwapFormats(*vk_swapchain.get())) {
            throw std::runtime_error("Swap chain image or depth format has changed!");
        }
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


