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

#include "quill/LogMacros.h"

using namespace Faye;

const uint32_t WIDTH = 1300;
const uint32_t HEIGHT = 900;

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

    void mainLoop() {        
        Input &input = Input::getInstance();

        while(!glfwWindow->shouldClose()) {
            vkData->timer.frameStart();
            glfwPollEvents();

            input.checkMovement(glfwWindow->getWindow(), camera, vkData->timer.getDelta());
            
            // Handle escape press, and close window/vk instance.
            if (input.isKeyPressed(glfwWindow->getWindow(), input.keyMap.escape)) {

                glfwSetWindowShouldClose(glfwWindow->getWindow(),true);
            }

            camera->update();

            vkData->drawFrame(*camera);

            // End frame statistics
            vkData->timer.frameEnd(Logger::getInstance());
        }

        vkDeviceWaitIdle(vkData->vk_device->getDevice());
    }

    void cleanup() {
        delete vkData;
        delete glfwWindow;
        delete camera;
    }
};