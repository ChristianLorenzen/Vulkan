#include <stdio.h>
#include <stdlib.h>
#include <exception>
#include <iostream>
#include <fstream>

#include "Window/Window.hpp"
#include "Vulkan/Vulkan.hpp"

#include "quill/Backend.h"
#include "quill/Frontend.h"
#include "quill/LogMacros.h"
#include "quill/Logger.h"
#include "quill/sinks/ConsoleSink.h"

using namespace Faye;

const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

class Engine {
    public: 
    Engine() {
        logger = quill::Frontend::create_or_get_logger(
            "root", quill::Frontend::create_or_get_sink<quill::ConsoleSink>("sink_id_1"));
    }
    void run() {
        int* test;
        test = 0;
        glfwWindow = new Window(WIDTH, HEIGHT, "[Faye] - Vulkan Renderer");
        LOG_INFO(logger, "Init Vulkan...");
        vkData = new Vulkan(glfwWindow);

        LOG_INFO(logger, "Starting main loop...");
        mainLoop();

        LOG_INFO(logger, "Starting cleanup...");
        cleanup();
    }
    void setLogger(quill::Logger** logger) {
        logger = logger;
    }
    private:
    // Custom class for glfw window related functionality.
    Window* glfwWindow;
    // Custom class for Vulkan init/functionality.
    Vulkan* vkData;
    quill::Logger* logger;

    void mainLoop() {        
        while(!glfwWindow->shouldClose()) {
            glfwPollEvents();
            vkData->drawFrame();
        }

        vkDeviceWaitIdle(vkData->getDevice());
    }

    void cleanup() {
        // Cleanup glfwWindow pointer;
        // delete vkData;
        delete glfwWindow;
        delete vkData;
    }
};

int main(int argc, char** argv) {
    quill::Backend::start();
    
    quill::Logger* logger = quill::Frontend::create_or_get_logger(
    "root", quill::Frontend::create_or_get_sink<quill::ConsoleSink>("sink_id_1"));

    Engine app;

    try {
        app.run();
    } catch (const std::exception& e) {
        LOG_ERROR(logger, "Error in Engine.run(): {}", e.what());
        return 1;
    }
    
    return 0;
}