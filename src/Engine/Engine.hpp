#include <stdio.h>
#include <stdlib.h>
#include <exception>
#include <iostream>
#include <fstream>

#include "Window/Window.hpp"
#include "Vulkan/Vulkan.hpp"
#include "Input/Input.hpp"
#include "Camera/Camera.hpp"

#include "quill/Backend.h"
#include "quill/Frontend.h"
#include "quill/LogMacros.h"
#include "quill/Logger.h"
#include "quill/sinks/ConsoleSink.h"

using namespace Faye;

const uint32_t WIDTH = 1300;
const uint32_t HEIGHT = 900;

class Engine {
    public: 
    Engine() {
        logger = quill::Frontend::create_or_get_logger(
            "root", quill::Frontend::create_or_get_sink<quill::ConsoleSink>("sink_id_1"));
    }
    void run() {
        glfwWindow = new Window(WIDTH, HEIGHT, "[Faye] - Vulkan Renderer");

        camera = new Camera();

        LOG_INFO(logger, "Init Vulkan...");
        vkData = new Vulkan(glfwWindow);

        LOG_INFO(logger, "Starting main loop...");
        mainLoop();

        LOG_INFO(logger, "Starting cleanup...");
        cleanup();
    }

    private:
    // Custom class for glfw window related functionality.
    Window* glfwWindow;
    // Custom class for Vulkan init/functionality.
    Vulkan* vkData;

    Camera* camera;

    quill::Logger* logger;

    void mainLoop() {        
        Input &input = Input::getInstance();

        while(!glfwWindow->shouldClose()) {
            vkData->timer.frameStart();
            glfwPollEvents();

            input.checkMovement(glfwWindow->getWindow(), camera, vkData->timer.getDelta());
            
            // Handle escape press, and close window/vk instance.
            if (input.isKeyPressed(glfwWindow->getWindow(), input.keyMap.escape)) {
                LOG_INFO(logger, "Escape pressed, exiting...");
                glfwSetWindowShouldClose(glfwWindow->getWindow(),true);
            }

            camera->update();

            vkData->drawFrame(*camera);

            // End frame statistics
            vkData->timer.frameEnd(logger);
        }

        vkDeviceWaitIdle(vkData->vk_device->getDevice());
    }

    void cleanup() {
        // Cleanup glfwWindow pointer;
        // delete vkData;
        delete vkData;
        delete glfwWindow;
        delete camera;
    }
};