#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3native.h>
#include <vulkan/vulkan.h>
#include <VKGLFW.hpp>
#include "Window.hpp"
#include <stdlib.h>
#include <iostream>

#include "quill/Backend.h"
#include "quill/Frontend.h"
#include "quill/LogMacros.h"
#include "quill/Logger.h"
#include "quill/sinks/ConsoleSink.h"
#include "Vulkan/Vulkan.hpp"

using Faye::Window;

static void framebufferResizeCallback(GLFWwindow* window, int width, int height) {
    //TODO: Wrap my head around how this can be fucking cast like this without throwing an error.
    auto app = reinterpret_cast<Faye::Vulkan*>(glfwGetWindowUserPointer(window));
    app->framebufferResized = true;
}

/// @brief Initialized glfw and creates a glfw window instance.
/// @param width Window width
/// @param height Window height
/// @param title The window name
Faye::Window::Window(uint32_t width, uint32_t height, const char* title) {
    LOG_INFO(Logger::getInstance(), "Window::Window - [Constructor] - Initializing glfw and the glfwWindow");

    WIDTH = width;
    HEIGHT = height;

    // Init GLFW 
    glfwInit();
    glfwSetErrorCallback(VKGLFW::glfwErrorCallback);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    //glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    window = glfwCreateWindow(width, height, title, nullptr, nullptr);
    glfwSetWindowUserPointer(window, this);
    glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);

    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window\n");
    } else {
        LOG_INFO(Logger::getInstance(), "GLFW Window created successfully.");
    }
}

/// @brief Destroys the glfwWindow instance, and terminated glfw.
/// Ensure only one Window instance is created so as to not call glfwTerminate early.
Window::~Window() {
    glfwDestroyWindow(window);
    glfwTerminate();
}

/// @brief Returns t/f based on if the current device supports Vulkan.
/// @return boolean 
bool Window::isVulkanSupported() {
    if(!glfwVulkanSupported()) {
        LOG_ERROR(Logger::getInstance(), "Vulkan not supported on this device.");
        glfwTerminate();
        return false;
    }
    return true;
}

/// @brief Returns the result of glfwWindowShouldClose()
/// @return boolean 
bool Window::shouldClose() {
    return glfwWindowShouldClose(window);
}

void Window::createWindowSurface(VkInstance instance, VkSurfaceKHR* surface) {
    if (glfwCreateWindowSurface(instance, window, nullptr, surface) != VK_SUCCESS) {
        throw std::runtime_error("Window.createWindowSurface() - Failed to create window surface.");
    }
}
