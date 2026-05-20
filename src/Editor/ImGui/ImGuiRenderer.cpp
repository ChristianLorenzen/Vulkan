#define GLFW_INCLUDE_VULKAN
#define VK_USER_PLATFORM_MACOS_MVK
#include <vulkan/vulkan.h>

#include <GLFW/glfw3.h>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"

#include "ImGuiRenderer.hpp"
#include "ImGuiCustomStyle.hpp"
#include "Renderer/Vulkan/vk_renderer.hpp"

#include "Core/Logging/Logger.hpp"

#include <cassert>
#include <fstream>
#include <stdexcept>

using namespace Faye;

void ImGuiRenderer::init(GLFWwindow *window,
                         VkInstance instance,
                         VkPhysicalDevice physicalDevice,
                         VkDevice device,
                         uint32_t queueFamily,
                         VkQueue queue,
                         VkDescriptorPool descriptorPool,
                         VkRenderPass renderPass,
                         uint32_t minImageCount,
                         uint32_t imageCount)
{
    assert(!initialized && "ImGuiRenderer already initialized");

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();

    const char *defaultFontPath = "src/include/fonts/Roboto-Regular.ttf";
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    std::ifstream fontFile(defaultFontPath);
    if (fontFile.good())
    {
        io.FontDefault = io.Fonts->AddFontFromFileTTF(defaultFontPath, 18.0f);
    }
    else
    {
        LOG_WARNING(
            Logger::getInstance(),
            "Could not load ImGui font file {}. Falling back to the default ImGui font.",
            defaultFontPath);
        io.FontDefault = io.Fonts->AddFontDefault();
    }

    ImGuiStyle &style = ImGui::GetStyle();
    style.FrameRounding = 2.0f;
    style.WindowRounding = 4.f;
    style.WindowBorderSize = 0.0f;

    ImGui::StyleColorsCustom();

    ImGui_ImplGlfw_InitForVulkan(window, true);

    ImGui_ImplVulkan_InitInfo info = {};
    info.Instance = instance;
    info.PhysicalDevice = physicalDevice;
    info.Device = device;
    info.QueueFamily = queueFamily;
    info.Queue = queue;
    info.DescriptorPool = descriptorPool;
    info.PipelineInfoMain.RenderPass = renderPass;
    info.MinImageCount = minImageCount;
    info.ImageCount = imageCount;

    ImGui_ImplVulkan_Init(&info);

    // Ensure font textures are uploaded before this function returns.
    vkDeviceWaitIdle(device);

    initialized = true;
}

void ImGuiRenderer::shutdown()
{
    if (!initialized)
        return;

    unregisterViewportTextures();
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    initialized = false;
}

void ImGuiRenderer::newFrame()
{
    assert(initialized && "ImGuiRenderer not initialized");
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void ImGuiRenderer::render(VkCommandBuffer commandBuffer)
{
    assert(initialized && "ImGuiRenderer not initialized");
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer, 0);
}

void ImGuiRenderer::registerViewportTextures(VulkanRenderer &renderer)
{
    if (!initialized)
        return;

    unregisterViewportTextures();

    VkSampler sampler = renderer.getSceneViewportSampler();
    if (sampler == VK_NULL_HANDLE)
        return;

    const auto sceneColorViews = renderer.getSceneImageViews();
    if (sceneColorViews.empty())
        return;

    sceneColorDescriptorSets.reserve(sceneColorViews.size());
    for (VkImageView view : sceneColorViews)
    {
        sceneColorDescriptorSets.push_back(
            ImGui_ImplVulkan_AddTexture(sampler, view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
    }

    for (VkImageView view : renderer.getSceneMotionImageViews())
    {
        sceneMotionDescriptorSets.push_back(
            ImGui_ImplVulkan_AddTexture(sampler, view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
    }

    for (VkImageView view : renderer.getSceneDepthImageViews())
    {
        sceneDepthDescriptorSets.push_back(
            ImGui_ImplVulkan_AddTexture(sampler, view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
    }

    const uint32_t targetCount = VulkanRenderer::kPostProcessTargetCount;
    postProcessDescriptorSets.resize(targetCount);
    for (uint32_t t = 0; t < targetCount; ++t)
    {
        for (VkImageView view : renderer.getPostProcessTargetImageViews(t))
        {
            postProcessDescriptorSets[t].push_back(
                ImGui_ImplVulkan_AddTexture(sampler, view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL));
        }
    }
}

void ImGuiRenderer::unregisterViewportTextures()
{
    for (auto ds : sceneColorDescriptorSets)
        ImGui_ImplVulkan_RemoveTexture(ds);
    sceneColorDescriptorSets.clear();

    for (auto ds : sceneMotionDescriptorSets)
        ImGui_ImplVulkan_RemoveTexture(ds);
    sceneMotionDescriptorSets.clear();

    for (auto ds : sceneDepthDescriptorSets)
        ImGui_ImplVulkan_RemoveTexture(ds);
    sceneDepthDescriptorSets.clear();

    for (auto &perTarget : postProcessDescriptorSets)
    {
        for (auto ds : perTarget)
            ImGui_ImplVulkan_RemoveTexture(ds);
    }
    postProcessDescriptorSets.clear();
}

VkDescriptorSet ImGuiRenderer::getSceneColorDS(uint32_t frameIdx) const
{
    if (frameIdx >= sceneColorDescriptorSets.size())
        return VK_NULL_HANDLE;
    return sceneColorDescriptorSets[frameIdx];
}

VkDescriptorSet ImGuiRenderer::getSceneMotionDS(uint32_t frameIdx) const
{
    if (frameIdx >= sceneMotionDescriptorSets.size())
        return VK_NULL_HANDLE;
    return sceneMotionDescriptorSets[frameIdx];
}

VkDescriptorSet ImGuiRenderer::getSceneDepthDS(uint32_t frameIdx) const
{
    if (frameIdx >= sceneDepthDescriptorSets.size())
        return VK_NULL_HANDLE;
    return sceneDepthDescriptorSets[frameIdx];
}

VkDescriptorSet ImGuiRenderer::getPostProcessDS(uint32_t targetIdx, uint32_t frameIdx) const
{
    if (targetIdx >= postProcessDescriptorSets.size())
        return VK_NULL_HANDLE;
    const auto &perTarget = postProcessDescriptorSets[targetIdx];
    if (frameIdx >= perTarget.size())
        return VK_NULL_HANDLE;
    return perTarget[frameIdx];
}
