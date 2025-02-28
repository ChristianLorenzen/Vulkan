#define VK_USER_PLATFORM_MACOS_MVK
#include <vulkan/vulkan.h>
#include <stdio.h>
#include <unordered_map>
#include <vector>

#include "Vulkan.hpp"
#include "Window/Window.hpp"
#include "Input/Input.hpp"
#include "vk_render_system.hpp"
#include "Structures/Vertex.hpp"
#include "Camera/Camera.hpp"
#include "Structures/GravitySystem.hpp"
#include "VulkanBuffer.hpp"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_vulkan.h"
#include "GUIComponents/imgui_Components.hpp"

#include "quill/LogMacros.h"

using namespace Faye;


const std::string MODEL_PATH = "src/include/viking_room.obj";
const std::string TEXTURE_PATH = "src/include/viking_room.png";


struct GlobalUBO {
    glm::mat4 projectionView{1.f};
    glm::vec3 lightDirection = glm::normalize(glm::vec3{1.f, -3.f, -1.f});
};

std::unique_ptr<Model> createCubeModel(VulkanDevice& device, glm::vec3 offset) {
    Builder builder{};

    // left face (white)
    Vertex v1 = {{-.5f, -.5f, -.5f}, {.9f, .9f, .9f}};
    Vertex v2 = {{-.5f, .5f, .5f}, {.9f, .9f, .9f}};
    Vertex v3 = {{-.5f, -.5f, .5f}, {.9f, .9f, .9f}};
    Vertex v4 = {{-.5f, .5f, -.5f}, {.9f, .9f, .9f}};

    // right face (yellow)
    Vertex v5 =  {{.5f, -.5f, -.5f}, {.8f, .8f, .1f}};
    Vertex v6 = {{.5f, .5f, .5f}, {.8f, .8f, .1f}};
    Vertex v7 = {{.5f, -.5f, .5f}, {.8f, .8f, .1f}};
    Vertex v8 =  {{.5f, .5f, -.5f}, {.8f, .8f, .1f}};

    // top face (orange, remember y axis points down)
    Vertex v9 = {{-.5f, -.5f, -.5f}, {.9f, .6f, .1f}};
    Vertex v10 = {{.5f, -.5f, .5f}, {.9f, .6f, .1f}};
    Vertex v11 = {{-.5f, -.5f, .5f}, {.9f, .6f, .1f}};
    Vertex v12 = {{.5f, -.5f, -.5f}, {.9f, .6f, .1f}};

    // bottom face (red)
    Vertex v13 = {{-.5f, .5f, -.5f}, {.8f, .1f, .1f}};
    Vertex v14 = {{.5f, .5f, .5f}, {.8f, .1f, .1f}};
    Vertex v15 = {{-.5f, .5f, .5f}, {.8f, .1f, .1f}};
    Vertex v16 = {{.5f, .5f, -.5f}, {.8f, .1f, .1f}};

    // nose face (blue)
    Vertex v17 = {{-.5f, -.5f, 0.5f}, {.1f, .1f, .8f}};
    Vertex v18 = {{.5f, .5f, 0.5f}, {.1f, .1f, .8f}};
    Vertex v19 = {{-.5f, .5f, 0.5f}, {.1f, .1f, .8f}};
    Vertex v20 = {{.5f, -.5f, 0.5f}, {.1f, .1f, .8f}};

    // tail face (green)
    Vertex v21 = {{-.5f, -.5f, -0.5f}, {.1f, .8f, .1f}};
    Vertex v22 = {{.5f, .5f, -0.5f}, {.1f, .8f, .1f}};
    Vertex v23 = {{-.5f, .5f, -0.5f}, {.1f, .8f, .1f}};
    Vertex v24 = {{.5f, -.5f, -0.5f}, {.1f, .8f, .1f}};
   
    std::vector<Vertex> vertices = {
        v1, v2, v3, v4, v5, v6, // left face
        v7, v8, v9, v10, v11, v12, // right face
        v13, v14, v15, v16, v17, v18, // top face
        v19, v20, v21, v22, v23, v24 // bottom face
    };

    builder.vertices = vertices;
    builder.indices = {
        0, 1, 2, 0, 3, 1, 4, 5, 6, 4, 7, 5, 8, 9, 10, 8, 11, 9, 12, 13, 14, 12, 15, 13, 16, 17, 18, 16, 19, 17,20, 21, 22, 20, 23, 21
    };

    for (auto& v : builder.vertices) {
      v.pos += offset;
    };
    return std::make_unique<Model>(device, builder);
  }


Faye::Vulkan::Vulkan(Window *win) : window{win}
{
    LOG_INFO(Logger::getInstance(), "Creating Vulkan Device class instance...");

    vk_device = new VulkanDevice(*window);

    vk_renderer = new VulkanRenderer(*window, *vk_device);
    globalPool = VulkanDescriptorPool::Builder(*vk_device)
        .setMaxSets(VulkanSwapchain::MAX_FRAMES_IN_FLIGHT)
        .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VulkanSwapchain::MAX_FRAMES_IN_FLIGHT)
        .build();
    loadGameObjects();

    imGUIPool = VulkanDescriptorPool::Builder(*vk_device)
        .setMaxSets(VulkanSwapchain::MAX_FRAMES_IN_FLIGHT)
        .addPoolSize(VK_DESCRIPTOR_TYPE_SAMPLER, VulkanSwapchain::MAX_FRAMES_IN_FLIGHT)
        .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VulkanSwapchain::MAX_FRAMES_IN_FLIGHT)
        .addPoolSize(VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, VulkanSwapchain::MAX_FRAMES_IN_FLIGHT)
        .addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VulkanSwapchain::MAX_FRAMES_IN_FLIGHT)
        .addPoolSize(VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, VulkanSwapchain::MAX_FRAMES_IN_FLIGHT)
        .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, VulkanSwapchain::MAX_FRAMES_IN_FLIGHT)
        .addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, VulkanSwapchain::MAX_FRAMES_IN_FLIGHT)
        .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VulkanSwapchain::MAX_FRAMES_IN_FLIGHT)
        .addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VulkanSwapchain::MAX_FRAMES_IN_FLIGHT)
        .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, VulkanSwapchain::MAX_FRAMES_IN_FLIGHT)
        .addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, VulkanSwapchain::MAX_FRAMES_IN_FLIGHT)
        .setPoolFlags(VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT)
        .build();

    vk_renderer->initImGui(imGUIPool->getPool());
}

void Faye::Vulkan::loadGameObjects() {
    std::shared_ptr<Model> model = Model::createModelFromFile(*vk_device, MODEL_PATH);
    auto go = GameObject::createGameObject();
    go.model = std::move(model);
    go.transform.translation = {0.f, 0.f, 2.5f};
    go.transform.rotation = glm::vec3(45.f, 90.f, 0.f);
    go.transform.scale = {.5f, .5f, .5f};
    gameObjects.push_back(std::move(go));
}

Faye::Vulkan::~Vulkan()
{
}

void Faye::Vulkan::run() {
    std::vector<std::unique_ptr<VulkanBuffer>> uboBuffers(VulkanSwapchain::MAX_FRAMES_IN_FLIGHT);
    for (int i = 0; i < uboBuffers.size(); i++) {
        uboBuffers[i] = std::make_unique<VulkanBuffer>(
            *vk_device,
            sizeof(GlobalUBO),
            1,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
        );
        uboBuffers[i]->map();
    }

    auto globalSetLayout = VulkanDescriptorSetLayout::Builder(*vk_device)
        .addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
        .build();

    std::vector<VkDescriptorSet> globalDescriptorSets(VulkanSwapchain::MAX_FRAMES_IN_FLIGHT);
    for (int i = 0; i < globalDescriptorSets.size(); i++) {
        auto bufferInfo = uboBuffers[i]->descriptorInfo();
        VulkanDescriptorWriter(*globalSetLayout, *globalPool)
            .writeBuffer(0, &bufferInfo)
            .build(globalDescriptorSets[i]);
    }

    Input &input = Input::getInstance();

    Camera camera{};
    camera.setViewDirection(glm::vec3(0.f), glm::vec3(0.5f, 0.f, 1.f));
    camera.setViewTarget(glm::vec3(-1.f, -2.f, 2.f), glm::vec3(0.f, 0.f, 0.f));
    auto viewObject = GameObject::createGameObject();

    auto lightObject = GameObject::createGameObject();

    SimpleRenderSystem simpleRenderSystem{*vk_device, vk_renderer->getSwapChainRenderPass(), globalSetLayout->getDescriptorSetLayout()};

    timer.frameStart();

    while(!window->shouldClose()) {
        glfwPollEvents();

        timer.frameEnd();

        input.moveInPlaneXZ(window->getWindow(), viewObject, timer.getDelta());
        camera.setViewYXZ(viewObject.transform.translation, viewObject.transform.rotation);

        float aspect = vk_renderer->getAspectRatio();
        camera.setPerspectiveProjection(glm::radians(50.f), aspect, 0.1f, 100.f);
        
        // Handle escape press, and close window/vk instance.
        if (input.isKeyPressed(window->getWindow(), input.keyMap.escape)) {

            glfwSetWindowShouldClose(window->getWindow(),true);
        }

        if (auto commandBuffer = vk_renderer->beginFrame()) {
            // update
            int frameIndex = vk_renderer->getFrameIndex();

            FrameInfo frameInfo {
                frameIndex,
                0,
                commandBuffer,
                camera,
                globalDescriptorSets[frameIndex]
            };

            GlobalUBO ubo{};
            ubo.projectionView = camera.getProjection() * camera.getView();
            uboBuffers[frameIndex]->writeToBuffer(&ubo);
            uboBuffers[frameIndex]->flush();
            // Other render passes will go here eventually
    
            // render
            vk_renderer->beginSwapchainRenderPass(commandBuffer);
    
            simpleRenderSystem.renderGameObjects(frameInfo, gameObjects);

            ImGui_ImplVulkan_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            //ImGui::ShowDemoWindow();

            Faye::Components::FrameCounter(Logger::getInstance(), timer.getFrameTime(1), timer.getAverageFPS());

            Faye::Components::CreateDockspace();

            ImGui::Render();

            ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer, 0);

            vk_renderer->endSwapchainRenderPass(commandBuffer);
    
            vk_renderer->endFrame();
        }
    }
    vkDeviceWaitIdle(vk_device->getDevice());
}