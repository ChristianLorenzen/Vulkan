#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <vulkan/vulkan.h>
#include "quill/Logger.h"


namespace Faye {
    class Window {
        public:
            Window(uint32_t width, uint32_t height, const char* title);
            ~Window();

            Window(const Window &) = delete;
            Window &operator=(const Window &) = delete;

            GLFWwindow* getWindow() { return window; }
            
            bool isVulkanSupported();
            bool shouldClose();
            void createWindowSurface(VkInstance instance, VkSurfaceKHR* surface);
        private:
            uint32_t WIDTH;
            uint32_t HEIGHT;
            GLFWwindow* window = nullptr;
            quill::Logger* logger;
    };
}
