#include <stdio.h>
#include <stdlib.h>
#include <exception>
#include <iostream>
#include <fstream>
#include <memory>

#include "Window/Window.hpp"
#include "Vulkan/Vulkan.hpp"
#include "Input/Input.hpp"
#include "Camera/Camera.hpp"
#include "Logging/Logger.hpp"
#include "Structures/Model.hpp"

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

    void loadModels()
    {
    }

    void mainLoop()
    {
        vkData->run();
    }
};