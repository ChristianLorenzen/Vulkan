#include "quill/Backend.h"
#include "quill/Frontend.h"
#include "quill/LogMacros.h"
#include "quill/Logger.h"
#include "quill/sinks/ConsoleSink.h"

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include "Camera/Camera.hpp"

#include <limits>

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
        
        int lookLeft = GLFW_KEY_LEFT;
        int lookRight = GLFW_KEY_RIGHT;
        int lookUp = GLFW_KEY_UP;
        int lookDown = GLFW_KEY_DOWN;

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

    void moveInPlaneXZ(GLFWwindow* window, Faye::GameObject& go, float dt) {
        glm::vec3 rotate{0};

        if (glfwGetKey(window, keyMap.lookLeft) == GLFW_PRESS) {
            rotate.y -= 1.f;
        }
        if (glfwGetKey(window, keyMap.lookRight) == GLFW_PRESS) {
            rotate.y += 1.f;
        }
        if (glfwGetKey(window, keyMap.lookUp) == GLFW_PRESS) {
            rotate.x += 1.f;
        }
        if (glfwGetKey(window, keyMap.lookDown) == GLFW_PRESS) {
            rotate.x -= 1.f;
        }

        if (glm::dot(rotate, rotate) > std::numeric_limits<float>::epsilon()) {
            go.transform.rotation += dt * glm::normalize(rotate);
        }

        go.transform.rotation.x = glm::clamp(go.transform.rotation.x, -1.5f, 1.5f);
        go.transform.rotation.y = glm::mod(go.transform.rotation.y, glm::two_pi<float>());

        float yaw = go.transform.rotation.y;
        const glm::vec3 forwardDir{sin(yaw), 0.f, cos(yaw)};
        const glm::vec3 rightDir{forwardDir.z, 0.f, -forwardDir.x};
        const glm::vec3 upDir{0.f, -1.f, 0.f};

        glm::vec3 move{0.f};
        if (glfwGetKey(window, keyMap.forward) == GLFW_PRESS) {
            move += forwardDir;
        }
        if (glfwGetKey(window, keyMap.backward) == GLFW_PRESS) {
            move -= forwardDir;
        }
        if (glfwGetKey(window, keyMap.left) == GLFW_PRESS) {
            move -= rightDir;
        }
        if (glfwGetKey(window, keyMap.right) == GLFW_PRESS) {
            move += rightDir;
        }

        if (glm::dot(move, move) > std::numeric_limits<float>::epsilon()) {
            go.transform.translation += 2 * dt * glm::normalize(move);
        }
        LOG_INFO(Logger::getInstance(), "Variables Move: {} {} {}, Rot: {} {} {}", move.x, move.y, move.z, rotate.x, rotate.y, rotate.z);
        LOG_INFO(Logger::getInstance(), "Move: {} {} {}, Rot: {} {} {}", go.transform.translation.x, go.transform.translation.y, go.transform.translation.z, go.transform.rotation.x, go.transform.rotation.y, go.transform.rotation.z);
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