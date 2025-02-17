#pragma once

#include "Window/Window.hpp"


namespace Faye {
    class Renderer {
    public:
        Renderer() = default;
        ~Renderer() = default;

        Renderer(const Renderer &) = delete;
		void operator=(const Renderer &) = delete;

        virtual void init();

        virtual void draw();

        Window *getWindow() {
            return window;
        }
    private:
        Window *window;
    };
}