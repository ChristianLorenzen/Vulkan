#pragma once

// Editor camera navigation (fly + orbit). This used to live inside
// Platform/Input/Input.hpp, which forced the platform layer to include scene
// types (Scene/Camera, Scene/Entities/Components). The platform layer now only
// accumulates raw input; the editor camera lives here, where depending on the
// scene is legitimate.

#include <algorithm>
#include <cstdlib>
#include <limits>
#include <string>

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

#include "Core/Logging/Logger.hpp"
#include "Platform/Input/Input.hpp"
#include "engine/Scene/Camera/Camera.hpp"
#include "engine/Scene/Entities/Components.hpp"
namespace
{
    bool isEditorCameraDebugLoggingEnabled()
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

namespace Faye::Editor
{
    class EditorCameraController
    {
    public:
        struct InputContext
        {
            bool viewportHovered = true;
            bool viewportFocused = true;
        };

        struct Settings
        {
            float moveSpeed = 4.0f;
            float zoomSpeed = 2.0f;
            float orbitSensitivity = 0.01f;
            float defaultOrbitDistance = 0.25f;
            float minOrbitDistance = 0.05f;
        };

        Settings settings;

        void update(
            GLFWwindow *window,
            Faye::TransformComponent &transform,
            float dt,
            InputContext context)
        {
            if (window == nullptr)
            {
                return;
            }

            updateMouseState(window);

            Faye::Input &input = Faye::Input::getInstance();
            const float scrollOffset = input.consumeScrollDelta();
            const bool middleMouseDown = isMouseButtonPressed(window, input.mouseMap.middle);
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
            if (context.viewportFocused && glfwGetKey(window, input.keyMap.forward) == GLFW_PRESS)
            {
                move += forwardDir;
            }
            if (context.viewportFocused && glfwGetKey(window, input.keyMap.backward) == GLFW_PRESS)
            {
                move -= forwardDir;
            }
            if (context.viewportFocused && glfwGetKey(window, input.keyMap.left) == GLFW_PRESS)
            {
                move -= rightDir;
            }
            if (context.viewportFocused && glfwGetKey(window, input.keyMap.right) == GLFW_PRESS)
            {
                move += rightDir;
            }
            if (context.viewportFocused && glfwGetKey(window, input.keyMap.up) == GLFW_PRESS)
            {
                move += upDir;
            }
            if (context.viewportFocused && glfwGetKey(window, input.keyMap.down) == GLFW_PRESS)
            {
                move -= upDir;
            }

            if (glm::dot(move, move) > std::numeric_limits<float>::epsilon())
            {
                const glm::vec3 translationDelta = settings.moveSpeed * dt * glm::normalize(move);
                transform.translation += translationDelta;

                if (hasOrbitPivot)
                {
                    orbitPivot += translationDelta;
                }
            }

            if (middleMouseOrbitActive)
            {
                transform.rotation.x = glm::clamp(
                    transform.rotation.x - static_cast<float>(mouseDelta.y) * settings.orbitSensitivity,
                    -1.5f,
                    1.5f);
                transform.rotation.y = glm::mod(
                    transform.rotation.y - static_cast<float>(mouseDelta.x) * settings.orbitSensitivity,
                    glm::two_pi<float>());

                forwardDir = Faye::Camera::forwardFromRotation(transform.rotation);
                rightDir = Faye::Camera::rightFromRotation(transform.rotation);
                upDir = Faye::Camera::upFromRotation(transform.rotation);
            }

            if (context.viewportHovered && scrollOffset != 0.0f)
            {
                initializeOrbitPivot(transform);
                orbitDistance = std::max(
                    settings.minOrbitDistance,
                    orbitDistance - scrollOffset * settings.zoomSpeed);
            }

            if (middleMouseOrbitActive || (context.viewportHovered && scrollOffset != 0.0f))
            {
                transform.translation = orbitPivot - forwardDir * orbitDistance;
            }

            if (isEditorCameraDebugLoggingEnabled())
            {
                LOG_INFO(Logger::get(), "Editor camera - move: {} {} {}, scroll: {}, mouseDelta: {} {}, orbitDistance: {}",
                         move.x, move.y, move.z, scrollOffset, mouseDelta.x, mouseDelta.y, orbitDistance);
                LOG_INFO(Logger::get(), "Editor camera - translation: {} {} {}, pivot: {} {} {}",
                         transform.translation.x, transform.translation.y, transform.translation.z,
                         orbitPivot.x, orbitPivot.y, orbitPivot.z);
            }
        }

        void update(GLFWwindow *window, Faye::TransformComponent &transform, float dt)
        {
            update(window, transform, dt, InputContext{});
        }

    private:
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

        bool isMouseButtonPressed(GLFWwindow *window, int button) const
        {
            return glfwGetMouseButton(window, button) == GLFW_PRESS;
        }

        void initializeOrbitPivot(const Faye::TransformComponent &transform)
        {
            if (!hasOrbitPivot)
            {
                orbitDistance = settings.defaultOrbitDistance;
                orbitPivot = transform.translation + Faye::Camera::forwardFromRotation(transform.rotation) * orbitDistance;
                hasOrbitPivot = true;
                return;
            }

            orbitDistance = std::max(
                settings.minOrbitDistance,
                glm::length(orbitPivot - transform.translation));
        }

        glm::vec<2, double> lastMousePos;
        glm::vec<2, double> mousePos;
        glm::vec<2, double> mouseDelta;
        bool hasMousePosition = false;
        glm::vec3 orbitPivot{0.0f};
        float orbitDistance = 0.0f;
        bool hasOrbitPivot = false;
        bool middleMouseOrbitActive = false;
    };
} // namespace Faye::Editor
