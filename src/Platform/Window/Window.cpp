#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>
#include "Platform/GLFW/VKGLFW.hpp"
#include "Platform/Input/Input.hpp"
#include "Window.hpp"
#include "Core/Logging/Logger.hpp"
#include <stdlib.h>
#include <iostream>

#include "quill/Backend.h"
#include "quill/Frontend.h"
#include "quill/LogMacros.h"
#include "quill/Logger.h"
#include "quill/sinks/ConsoleSink.h"

using Faye::Window;

// static void framebufferResizeCallback(GLFWwindow* window, int width, int height) {
//     auto app = reinterpret_cast<Faye::Vulkan*>(glfwGetWindowUserPointer(window));
//     app->framebufferResized = true;
// }

void Faye::Window::framebufferResizeCallback(GLFWwindow *window, int width, int height)
{
    auto win = reinterpret_cast<Faye::Window *>(glfwGetWindowUserPointer(window));
    win->framebufferResized = true;
    win->width = width;
    win->height = height;
}

/// @brief Initialized glfw and creates a glfw window instance.
/// @param width Window width
/// @param height Window height
/// @param title The window name
Faye::Window::Window(uint32_t width, uint32_t height, const char *title) : width{width}, height{height}, title{title}
{
    LOG_INFO(Logger::get(), "Window::Window - [Constructor] - Initializing glfw and the glfwWindow");

    // Init GLFW
    glfwInit();
    glfwSetErrorCallback(VKGLFW::glfwErrorCallback);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
    glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_FALSE);
    glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GLFW_TRUE);

    int monitorCount;
    GLFWmonitor **monitors = glfwGetMonitors(&monitorCount);

    // TODO: Eventually remove this logic, but for now I want to open this on side monitor while doing dev
    window = glfwCreateWindow(width, height, title, nullptr, nullptr);

    // TODO: dev convenience — open on the side monitor.
    if (monitorCount > 1) {
        int mx, my, mw, mh;
        glfwGetMonitorWorkarea(monitors[1], &mx, &my, &mw, &mh);
        glfwSetWindowPos(window, mx + (mw - width) / 2, my + (mh - height) / 2);
    }

    glfwShowWindow(window);

    if (!window)
    {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window\n");
    }
    else
    {
        glfwSetWindowUserPointer(window, this);
        glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
        glfwSetKeyCallback(window, Input::keyCallback);
        glfwSetCursorPosCallback(window, Input::cursorCallback);
        glfwSetMouseButtonCallback(window, Input::mouseButtonCallback);
        glfwSetScrollCallback(window, Input::scrollCallback);
        glfwSetWindowSizeLimits(window, 800, 600, GLFW_DONT_CARE, GLFW_DONT_CARE);

        LOG_INFO(Logger::get(), "GLFW Window created successfully.");
    }
}

/// @brief Destroys the glfwWindow instance, and terminated glfw.
/// Ensure only one Window instance is created so as to not call glfwTerminate early.
Window::~Window()
{
    glfwDestroyWindow(window);
    glfwTerminate();
}

/// @brief Returns t/f based on if the current device supports Vulkan.
/// @return boolean
bool Window::isVulkanSupported()
{
    if (!glfwVulkanSupported())
    {
        LOG_ERROR(Logger::get(), "Vulkan not supported on this device.");
        glfwTerminate();
        return false;
    }
    return true;
}

/// @brief Returns the result of glfwWindowShouldClose()
/// @return boolean
bool Window::shouldClose()
{
    return glfwWindowShouldClose(window);
}

VkExtent2D Window::getExtent()
{
    int framebufferWidth = 0;
    int framebufferHeight = 0;
    glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);

    width = static_cast<uint32_t>(std::max(framebufferWidth, 0));
    height = static_cast<uint32_t>(std::max(framebufferHeight, 0));

    return {width, height};
}

void Window::createWindowSurface(VkInstance instance, VkSurfaceKHR *surface)
{
    if (glfwCreateWindowSurface(instance, window, nullptr, surface) != VK_SUCCESS)
    {
        throw std::runtime_error("Window.createWindowSurface() - Failed to create window surface.");
    }
}
