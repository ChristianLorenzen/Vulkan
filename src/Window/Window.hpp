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
            
            VkExtent2D getExtent() { return {static_cast<uint32_t>(width), static_cast<uint32_t>(height)}; }

            bool isVulkanSupported();
            bool shouldClose();
            void createWindowSurface(VkInstance instance, VkSurfaceKHR* surface);

            bool wasWindowResized() { return framebufferResized; }
            void resetWindowResizedFlag() { framebufferResized = false; }

        private:
            static void framebufferResizeCallback(GLFWwindow* window, int width, int height);

            uint32_t width;
            uint32_t height;
            const char* title;

            bool framebufferResized = false;

            GLFWwindow* window = nullptr;
    };
}
