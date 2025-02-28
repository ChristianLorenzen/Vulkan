#include <stdio.h>
#include <stdlib.h>
#include <exception>
#include <iostream>
#include <fstream>

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

struct SimplePushConstantData {
    glm::vec2 offset;
    glm::vec3 color;
};


class Engine {
    public: 
    Engine() {}
    void run() {
        glfwWindow = new Window(WIDTH, HEIGHT, "[Faye] - Vulkan Renderer");

        camera = new Camera();

        LOG_INFO(Logger::getInstance(), "Init Vulkan...");

        vkData = new Vulkan(glfwWindow);

        LOG_INFO(Logger::getInstance(), "Starting main loop...");
        mainLoop();

        LOG_INFO(Logger::getInstance(), "Starting cleanup...");
        cleanup();
    }

    private:
    // Custom class for glfw window related functionality.
    Window* glfwWindow;

    // Custom class for Vulkan init/functionality.
    Vulkan* vkData;

    Camera* camera;

    FrameTimer* timer;

    void loadModels() {

    }

    void mainLoop() {        
        vkData->run();
    }

    void cleanup() {
        delete vkData;
        delete glfwWindow;
        delete camera;
    }
};