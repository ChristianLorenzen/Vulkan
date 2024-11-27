#include <stdio.h>
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#include <stdlib.h>
#include <exception>
#include <iostream>

const uint32_t WIDTH = 800;
const uint32_t HEIGHT = 600;

class Engine {
    public: 
    void run() {
        initGLFW();
        initVulkan();
        mainLoop();
        cleanup();
    }

    private:
    GLFWwindow* window;
    VkInstance instance;
    void initGLFW() {
        glfwInit();

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);

    }
    void initVulkan() {
        VkApplicationInfo appInfo = {};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Hello Triangle";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "No Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_0;

        VkInstanceCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;

        uint32_t extensionCount = 3;
        const char** extensions = (const char**)malloc(sizeof(const char*)* 3);
        extensions[0] = "VK_KHR_surface";
        extensions[1] = "VK_EXT_metal_surface";
        extensions[2] = "VK_KHR_portability_enumeration";

        createInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;

        createInfo.enabledExtensionCount = extensionCount;
        createInfo.ppEnabledExtensionNames = extensions;

        VkResult result = vkCreateInstance(&createInfo, NULL, &instance);

        if (result != VK_SUCCESS) {
            printf("Failed to create instance: %d\n", result);

            throw std::runtime_error("Failed to create instance");
        }
    }

    void mainLoop() {
        while(!glfwWindowShouldClose(window)) {
            glfwPollEvents();
        }
    }
    void cleanup() {
        glfwDestroyWindow(window);
        glfwTerminate();
    }
};

int main(int argc, char** argv) {
    Engine app;

    try {
        app.run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}