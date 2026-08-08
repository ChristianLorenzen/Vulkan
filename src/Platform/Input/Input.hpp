#pragma once

#include "quill/Backend.h"
#include "quill/Frontend.h"
#include "quill/LogMacros.h"
#include "quill/Logger.h"
#include "quill/sinks/ConsoleSink.h"

#include <GLFW/glfw3.h>

#include "Core/Logging/Logger.hpp"

#include <cstdlib>
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
    // Raw platform input: GLFW callback plumbing, key/mouse maps, and
    // accumulated scroll state. Deliberately free of scene and editor types.
    // Editor camera navigation reads raw state from here but lives in
    // Editor/Utility/EditorCameraController.hpp.
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

        KeyMap keyMap;
        MouseMap mouseMap;

        // Scroll delta accumulated by scrollCallback since the last consume.
        // Consumed each frame by the editor camera controller.
        float consumeScrollDelta()
        {
            float currentScrollDelta = scrollDelta;
            scrollDelta = 0.0f;
            return currentScrollDelta;
        }

        static void keyCallback(GLFWwindow *window, int key, int scancode, int action, int mods)
        {
            getInstance().keyCallbackImpl(window, key, scancode, action, mods);
        }

        static void cursorCallback(GLFWwindow *window, double x, double y)
        {
            getInstance().cursorCallbackImpl(window, x, y);
        }

        static void mouseButtonCallback(GLFWwindow *window, int button, int action, int mods)
        {
            getInstance().mouseButtonCallbackImpl(window, button, action, mods);
        }

        static void scrollCallback(GLFWwindow *window, double xoffset, double yoffset)
        {
            getInstance().scrollCallbackImpl(window, xoffset, yoffset);
        }

        bool isKeyPressed(GLFWwindow *window, int key)
        {
            // TODO: This might screw things up if not on main thread.
            return glfwGetKey(window, key) == GLFW_PRESS;
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

    private:
        Input(void) {}
        Input(Input const &);
        void operator=(Input const &);
        quill::Logger *logger = nullptr;
        float scrollDelta = 0.0f;
    };
}; // namespace Faye
