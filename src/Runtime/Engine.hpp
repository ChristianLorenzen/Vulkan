#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <exception>
#include <iostream>
#include <fstream>
#include <memory>

#include "Core/Logging/Logger.hpp"
#include "Core/Time/FrameTimer.hpp"
#include "Platform/Input/Input.hpp"
#include "Platform/Window/Window.hpp"
#include "Renderer/Vulkan/Vulkan.hpp"
#include "Scene/Camera/Camera.hpp"
#include "Scene/Entities/GameObject.hpp"

#include "quill/LogMacros.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

using namespace Faye;

const uint32_t WIDTH = 1300;
const uint32_t HEIGHT = 900;

struct SimplePushConstantData
{
    glm::vec2 offset;
    glm::vec3 color;
};

class Engine
{
public:
    Engine() = default;

    void run()
    {
        glfwWindow = std::make_unique<Window>(WIDTH, HEIGHT, "[Faye] - Vulkan Renderer");

        camera = std::make_unique<Camera>();

        LOG_INFO(Logger::getInstance(), "Init Vulkan...");

        vkData = std::make_unique<Vulkan>(*glfwWindow);

        LOG_INFO(Logger::getInstance(), "Starting main loop...");
        mainLoop();
    }

private:
    // Custom class for glfw window related functionality.
    std::unique_ptr<Window> glfwWindow;

    // Custom class for Vulkan init/functionality.
    std::unique_ptr<Vulkan> vkData;

    std::unique_ptr<Camera> camera;
    FrameTimer timer;
    GameObject cameraController = GameObject::createGameObject();

    void loadModels()
    {
    }

    void mainLoop()
    {
        Input &input = Input::getInstance();

        timer.frameStart();

        while (!glfwWindow->shouldClose())
        {
            glfwPollEvents();

            timer.frameEnd();

            input.moveInPlaneXZ(glfwWindow->getWindow(), cameraController, static_cast<float>(timer.getDelta()));
            camera->setViewYXZ(cameraController.transform.translation, cameraController.transform.rotation);
            camera->setPerspectiveProjection(glm::radians(50.f), vkData->getAspectRatio(), 0.1f, 100.f);

            if (input.isKeyPressed(glfwWindow->getWindow(), input.keyMap.escape))
            {
                glfwSetWindowShouldClose(glfwWindow->getWindow(), true);
            }

            VulkanFrameInput frameInput{
                *camera,
                static_cast<int>(timer.getFrameTime(1)),
                static_cast<int>(timer.getAverageFPS())};

            vkData->renderFrame(frameInput);
        }
    }
};