#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <exception>
#include <iostream>
#include <fstream>
#include <memory>

#include "Assets/ModelRegistry.hpp"
#include "Core/Logging/Logger.hpp"
#include "Core/Time/FrameTimer.hpp"
#include "Platform/Input/Input.hpp"
#include "Platform/Window/Window.hpp"
#include "Renderer/Scene/RenderExtractionManager.hpp"
#include "Renderer/Resources/Model.hpp"
#include "Renderer/View/RenderView.hpp"
#include "Renderer/Vulkan/Vulkan.hpp"
#include "Scene/SceneManager.hpp"

#include "quill/LogMacros.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

using namespace Faye;

const uint32_t WIDTH = 1300;
const uint32_t HEIGHT = 900;

class Engine
{
public:
    Engine() = default;

    void run()
    {
        glfwWindow = std::make_unique<Window>(WIDTH, HEIGHT, "[Faye] - Vulkan Renderer");

        LOG_INFO(Logger::getInstance(), "Init Vulkan...");

        vkData = std::make_unique<Vulkan>(*glfwWindow);
        modelRegistry = std::make_unique<ModelRegistry>();
        renderExtractionManager = std::make_unique<RenderExtractionManager>();
        sceneManager = std::make_unique<SceneManager>();
        initializeScene();

        LOG_INFO(Logger::getInstance(), "Starting main loop...");
        mainLoop();
    }

private:
    // Custom class for glfw window related functionality.
    std::unique_ptr<Window> glfwWindow;

    // Custom class for Vulkan init/functionality.
    std::unique_ptr<Vulkan> vkData;

    std::unique_ptr<ModelRegistry> modelRegistry;
    std::unique_ptr<RenderExtractionManager> renderExtractionManager;
    std::unique_ptr<SceneManager> sceneManager;
    FrameTimer timer;
    Scene::EntityId activeCameraEntity = Scene::invalidEntity;

    void initializeScene()
    {
        Scene &scene = sceneManager->createScene("Main Scene");

        activeCameraEntity = scene.createEntity();
        scene.addTransform(activeCameraEntity);
        scene.addCamera(activeCameraEntity, true);

        auto meshEntity = scene.createEntity();
        auto &meshTransform = scene.addTransform(meshEntity);
        auto meshModel = Model::createModelFromFile(*vkData->getVkDevice(), "src/include/viking_room.obj");
        auto meshHandle = modelRegistry->registerModel(std::move(meshModel));
        auto &meshComponent = scene.addMesh(
            meshEntity,
            meshHandle);
        meshTransform.translation = {0.f, 0.f, 2.5f};
        meshTransform.rotation = glm::vec3(45.f, 90.f, 0.f);
        meshTransform.scale = {.5f, .5f, .5f};
        meshComponent.color = {1.f, 1.f, 1.f};
    }

    void mainLoop()
    {
        Input &input = Input::getInstance();
        Scene &scene = sceneManager->getActiveScene();

        timer.frameStart();

        while (!glfwWindow->shouldClose())
        {
            glfwPollEvents();

            timer.frameEnd();

            auto *cameraTransform = scene.tryGetTransform(activeCameraEntity);
            auto *cameraComponent = scene.getPrimaryCamera();
            if (cameraTransform == nullptr || cameraComponent == nullptr)
            {
                throw std::runtime_error("Active scene camera is not configured correctly");
            }

            VkExtent2D windowExtent = glfwWindow->getExtent();
            RenderView renderView{
                &cameraComponent->camera,
                {windowExtent.width, windowExtent.height},
                RenderOutputTarget::Swapchain,
                RenderDebugMode::Lit};

            input.moveInPlaneXZ(glfwWindow->getWindow(), *cameraTransform, static_cast<float>(timer.getDelta()));
            cameraComponent->camera.setViewYXZ(cameraTransform->translation, cameraTransform->rotation);
            cameraComponent->camera.setPerspectiveProjection(glm::radians(50.f), renderView.viewport.aspectRatio(), 0.1f, 100.f);

            if (input.isKeyPressed(glfwWindow->getWindow(), input.keyMap.escape))
            {
                glfwSetWindowShouldClose(glfwWindow->getWindow(), true);
            }

            RenderSceneSnapshot renderScene = renderExtractionManager->extract(scene, *modelRegistry);

            VulkanFrameInput frameInput{
                renderView,
                renderScene,
                static_cast<int>(timer.getFrameTime(1)),
                static_cast<int>(timer.getAverageFPS())};

            vkData->renderFrame(frameInput);
        }
    }
};