#pragma once

#include "quill/Backend.h"
#include "quill/Frontend.h"
#include "quill/LogMacros.h"
#include "quill/Logger.h"
#include "quill/sinks/ConsoleSink.h"

#include <GLFW/glfw3.h>
#include <glm/gtc/constants.hpp>

#include "Scene/Camera/Camera.hpp"
#include "Scene/Entities/Components.hpp"

#include <algorithm>
#include <cstdlib>
#include <limits>
#include <string>

namespace
{
    bool isInputDebugLoggingEnabled()
    {
        static const bool enabled = []
        {
            const char *inputDebugOverride = std::getenv("FAYE_INPUT_DEBUG");
            if (inputDebugOverride == nullptr)
            {
                return false;
            }

            std::string value = inputDebugOverride;
            return value == "1" || value == "true" || value == "TRUE" || value == "on" || value == "ON";
        }();

        return enabled;
    }
} // namespace

namespace Faye
{
    class Input
    {
    public:
        static Input &getInstance()
        {
            static Input instance;
            return instance;
        }

        struct KeyMap
        {
            int left = GLFW_KEY_A;
            int right = GLFW_KEY_D;
            int forward = GLFW_KEY_W;
            int backward = GLFW_KEY_S;
            int up = GLFW_KEY_E;
            int down = GLFW_KEY_Q;

            int escape = GLFW_KEY_ESCAPE;
        };

        struct MouseMap
        {
            int left = GLFW_MOUSE_BUTTON_LEFT;
            int right = GLFW_MOUSE_BUTTON_RIGHT;
            int middle = GLFW_MOUSE_BUTTON_MIDDLE;
        };

        struct EditorCameraInputContext
        {
            bool viewportHovered = true;
            bool viewportFocused = true;
        };

        struct EditorCameraSettings
        {
            float moveSpeed = 4.0f;
            float zoomSpeed = 2.0f;
            float orbitSensitivity = 0.01f;
            float defaultOrbitDistance = 0.25f;
            float minOrbitDistance = 0.05f;
        };

        KeyMap keyMap;
        MouseMap mouseMap;
        EditorCameraSettings editorCameraSettings;

        void updateEditorCamera(
            GLFWwindow *window,
            Faye::TransformComponent &transform,
            float dt,
            EditorCameraInputContext context)
        {
            if (window == nullptr)
            {
                return;
            }

            updateMouseState(window);

            const float scrollOffset = consumeScrollDelta();
            const bool middleMouseDown = isMouseButtonPressed(window, mouseMap.middle);
            if (middleMouseDown && context.viewportHovered && !middleMouseOrbitActive)
            {
                initializeOrbitPivot(transform);
                middleMouseOrbitActive = true;
            }
            else if (!middleMouseDown)
            {
                middleMouseOrbitActive = false;
            }

            glm::vec3 forwardDir = Faye::Camera::forwardFromRotation(transform.rotation);
            glm::vec3 rightDir = Faye::Camera::rightFromRotation(transform.rotation);
            glm::vec3 upDir = Faye::Camera::upFromRotation(transform.rotation);

            glm::vec3 move{0.f};
            if (context.viewportFocused && glfwGetKey(window, keyMap.forward) == GLFW_PRESS)
            {
                move += forwardDir;
            }
            if (context.viewportFocused && glfwGetKey(window, keyMap.backward) == GLFW_PRESS)
            {
                move -= forwardDir;
            }
            if (context.viewportFocused && glfwGetKey(window, keyMap.left) == GLFW_PRESS)
            {
                move -= rightDir;
            }
            if (context.viewportFocused && glfwGetKey(window, keyMap.right) == GLFW_PRESS)
            {
                move += rightDir;
            }
            if (context.viewportFocused && glfwGetKey(window, keyMap.up) == GLFW_PRESS)
            {
                move += upDir;
            }
            if (context.viewportFocused && glfwGetKey(window, keyMap.down) == GLFW_PRESS)
            {
                move -= upDir;
            }

            if (glm::dot(move, move) > std::numeric_limits<float>::epsilon())
            {
                const glm::vec3 translationDelta = editorCameraSettings.moveSpeed * dt * glm::normalize(move);
                transform.translation += translationDelta;

                if (hasOrbitPivot)
                {
                    orbitPivot += translationDelta;
                }
            }

            if (middleMouseOrbitActive)
            {
                transform.rotation.x = glm::clamp(
                    transform.rotation.x - static_cast<float>(mouseDelta.y) * editorCameraSettings.orbitSensitivity,
                    -1.5f,
                    1.5f);
                transform.rotation.y = glm::mod(
                    transform.rotation.y - static_cast<float>(mouseDelta.x) * editorCameraSettings.orbitSensitivity,
                    glm::two_pi<float>());

                forwardDir = Faye::Camera::forwardFromRotation(transform.rotation);
                rightDir = Faye::Camera::rightFromRotation(transform.rotation);
                upDir = Faye::Camera::upFromRotation(transform.rotation);
            }

            if (context.viewportHovered && scrollOffset != 0.0f)
            {
                initializeOrbitPivot(transform);
                orbitDistance = std::max(
                    editorCameraSettings.minOrbitDistance,
                    orbitDistance - scrollOffset * editorCameraSettings.zoomSpeed);
            }

            if (middleMouseOrbitActive || (context.viewportHovered && scrollOffset != 0.0f))
            {
                transform.translation = orbitPivot - forwardDir * orbitDistance;
            }

            if (isInputDebugLoggingEnabled())
            {
                LOG_INFO(Logger::getInstance(), "Editor camera - move: {} {} {}, scroll: {}, mouseDelta: {} {}, orbitDistance: {}",
                         move.x, move.y, move.z, scrollOffset, mouseDelta.x, mouseDelta.y, orbitDistance);
                LOG_INFO(Logger::getInstance(), "Editor camera - translation: {} {} {}, pivot: {} {} {}",
                         transform.translation.x, transform.translation.y, transform.translation.z,
                         orbitPivot.x, orbitPivot.y, orbitPivot.z);
            }
        }

        void updateEditorCamera(GLFWwindow *window, Faye::TransformComponent &transform, float dt)
        {
            updateEditorCamera(window, transform, dt, EditorCameraInputContext{});
        }

        void moveInPlaneXZ(GLFWwindow *window, Faye::TransformComponent &transform, float dt)
        {
            updateEditorCamera(window, transform, dt);
        }

        static void keyCallback(GLFWwindow *window, int key, int scancode, int action, int mods)
        {
            getInstance().keyCallbackImpl(window, key, scancode, action, mods);
        }

        void keyCallbackImpl(GLFWwindow *window, int key, int scancode, int action, int mods)
        {
            (void)window;
            (void)scancode;
            (void)mods;

            if (!isInputDebugLoggingEnabled())
            {
                return;
            }

            logger = quill::Frontend::create_or_get_logger(
                "root", quill::Frontend::create_or_get_sink<quill::ConsoleSink>("sink_id_1"));

            LOG_INFO(logger, "Input - keyCallback - key: {} action: {}", key, action);
        }

        static void cursorCallback(GLFWwindow *window, double x, double y)
        {
            getInstance().cursorCallbackImpl(window, x, y);
        }

        void cursorCallbackImpl(GLFWwindow *window, double x, double y)
        {
            (void)window;

            if (!isInputDebugLoggingEnabled())
            {
                return;
            }

            logger = quill::Frontend::create_or_get_logger(
                "root", quill::Frontend::create_or_get_sink<quill::ConsoleSink>("sink_id_1"));

            LOG_INFO(logger, "Input - cursorCallback - x: {} y: {}", x, y);
        }

        static void mouseButtonCallback(GLFWwindow *window, int button, int action, int mods)
        {
            getInstance().mouseButtonCallbackImpl(window, button, action, mods);
        }

        void mouseButtonCallbackImpl(GLFWwindow *window, int button, int action, int mods)
        {
            (void)window;

            if (!isInputDebugLoggingEnabled())
            {
                return;
            }

            logger = quill::Frontend::create_or_get_logger(
                "root", quill::Frontend::create_or_get_sink<quill::ConsoleSink>("sink_id_1"));

            LOG_INFO(logger, "Input - mouseButtonCallback - button: {} action: {} mods: {}", button, action, mods);
        }

        static void scrollCallback(GLFWwindow *window, double xoffset, double yoffset)
        {
            getInstance().scrollCallbackImpl(window, xoffset, yoffset);
        }

        void scrollCallbackImpl(GLFWwindow *window, double xoffset, double yoffset)
        {
            (void)window;

            scrollDelta += static_cast<float>(yoffset);

            if (!isInputDebugLoggingEnabled())
            {
                return;
            }

            logger = quill::Frontend::create_or_get_logger(
                "root", quill::Frontend::create_or_get_sink<quill::ConsoleSink>("sink_id_1"));

            LOG_INFO(logger, "Input - scrollCallback - xoffset: {} yoffset: {}", xoffset, yoffset);
        }

        bool isKeyPressed(GLFWwindow *window, int key)
        {
            // TODO: This might screw things up if not on main thread.
            return glfwGetKey(window, key) == GLFW_PRESS;
        }

    private:
        Input(void) {}
        Input(Input const &);
        void operator=(Input const &);
        quill::Logger *logger = nullptr;

        void updateMouseState(GLFWwindow *window)
        {
            glfwGetCursorPos(window, &mousePos.x, &mousePos.y);

            if (!hasMousePosition)
            {
                lastMousePos = mousePos;
                hasMousePosition = true;
            }

            mouseDelta = mousePos - lastMousePos;
            lastMousePos = mousePos;
        }

        float consumeScrollDelta()
        {
            float currentScrollDelta = scrollDelta;
            scrollDelta = 0.0f;
            return currentScrollDelta;
        }

        bool isMouseButtonPressed(GLFWwindow *window, int button) const
        {
            return glfwGetMouseButton(window, button) == GLFW_PRESS;
        }

        void initializeOrbitPivot(const Faye::TransformComponent &transform)
        {
            if (!hasOrbitPivot)
            {
                orbitDistance = editorCameraSettings.defaultOrbitDistance;
                orbitPivot = transform.translation + Faye::Camera::forwardFromRotation(transform.rotation) * orbitDistance;
                hasOrbitPivot = true;
                return;
            }

            orbitDistance = std::max(
                editorCameraSettings.minOrbitDistance,
                glm::length(orbitPivot - transform.translation));
        }

        glm::vec<2, double> lastMousePos;
        glm::vec<2, double> mousePos;
        glm::vec<2, double> mouseDelta;
        bool hasMousePosition = false;
        float scrollDelta = 0.0f;
        glm::vec3 orbitPivot{0.0f};
        float orbitDistance = 0.0f;
        bool hasOrbitPivot = false;
        bool middleMouseOrbitActive = false;
    };
};