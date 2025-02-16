#include "quill/Backend.h"
#include "quill/Frontend.h"
#include "quill/LogMacros.h"
#include "quill/Logger.h"
#include "quill/sinks/ConsoleSink.h"

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include "Camera/Camera.hpp"

class Input {
    public:
    static Input& getInstance() {
        static Input instance;
        return instance;
    }

    struct KeyMap {
        int left = GLFW_KEY_A;
        int right = GLFW_KEY_D;
        int forward = GLFW_KEY_W;
        int backward = GLFW_KEY_S;

        int escape = GLFW_KEY_ESCAPE;
    };

    struct MouseMap {
        int left = GLFW_MOUSE_BUTTON_1;
        int right = GLFW_MOUSE_BUTTON_2;
    };

    KeyMap keyMap;
    MouseMap mouseMap;

    void checkMovement(GLFWwindow* window, Faye::Camera* camera, double delta) {
        if (glfwGetKey(window, keyMap.forward) == GLFW_PRESS) {
            camera->velocity.y += .5f * delta;
        }
        if (glfwGetKey(window, keyMap.backward) == GLFW_PRESS) {
            camera->velocity.y -= .5f * delta;
        }
        if (glfwGetKey(window, keyMap.left) == GLFW_PRESS) {
            camera->velocity.x -= .5f * delta;
        }
        if (glfwGetKey(window, keyMap.right) == GLFW_PRESS) {
            camera->velocity.x += .5f * delta;
        }

        lastMousePos = mousePos;

        glfwGetCursorPos(window, &mousePos.x, &mousePos.y);

        camera->yaw += (lastMousePos.x - mousePos.x) * 0.01f;
        camera->pitch -= (lastMousePos.y - mousePos.y) * 0.01f;
    }

    static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
        getInstance().keyCallbackImpl(window, key, scancode, action, mods);
    }

    void keyCallbackImpl(GLFWwindow* window, int key, int scancode, int action, int mods) {
        logger = quill::Frontend::create_or_get_logger(
        "root", quill::Frontend::create_or_get_sink<quill::ConsoleSink>("sink_id_1"));

        LOG_INFO(logger, "Input - keyCallback - key: {} action: {}", key, action);
    }

    static void cursorCallback(GLFWwindow* window, double x, double y) {
        getInstance().cursorCallbackImpl(window, x, y);
    }

    void cursorCallbackImpl(GLFWwindow* window, double x, double y) {
        logger = quill::Frontend::create_or_get_logger(
        "root", quill::Frontend::create_or_get_sink<quill::ConsoleSink>("sink_id_1"));

        LOG_INFO(logger, "Input - cursorCallback - x: {} y: {}", x, y);

        // lastMousePos = mousePos;

        // mousePos = glm::vec2(x, y);
    }

    bool isKeyPressed(GLFWwindow* window, int key) {
        // TODO: This might screw things up if not on main thread.
        return glfwGetKey(window, key) == GLFW_PRESS;
    }

    private:
        Input(void){}
        Input(Input const &);
        void operator=(Input const &);
        quill::Logger* logger;

        glm::vec<2, double> lastMousePos;
        glm::vec<2, double> mousePos;

};