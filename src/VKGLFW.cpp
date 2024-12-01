#include <iostream>
#include "VKGLFW.hpp"

using namespace Faye;

void Faye::VKGLFW::glfwErrorCallback(int error, const char* description) {
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
};
