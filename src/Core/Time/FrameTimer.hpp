#pragma once

#include <ctime>
#include <deque>
#include <chrono>
#include <array>
#include <iostream>
#include <algorithm>

#include "quill/Backend.h"
#include "quill/Frontend.h"
#include "quill/LogMacros.h"
#include "quill/Logger.h"
#include "quill/sinks/ConsoleSink.h"

namespace Faye {
    class FrameTimer {
    public:
        // Called at the start of a frame
        void frameStart() {
            startTime = std::chrono::high_resolution_clock::now();
        }

        void drawStart() {
            drawStartTime = std::chrono::high_resolution_clock::now();
        }

        void drawEnd() {
            auto endTime = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration<float, std::chrono::seconds::period>(endTime - drawStartTime);
            std::cout << "Render time: " << duration.count() << " ms" << std::endl;
        }

        // Called at the end of a frame
        // TODO: Fix some funky stuff going on here with FPS calc
        void frameEnd() {
            auto endTime = std::chrono::high_resolution_clock::now();

            // Compute frametime in seconds
            delta = std::chrono::duration<float, std::chrono::seconds::period>(endTime - startTime);
            double currentFrameTime = delta.count() * 1000.0;

            startTime = endTime;

            // Update frametime history in milliseconds
            frameTimes[frameIndex] = currentFrameTime;
            frameIndex = (frameIndex + 1) % frameTimes.size();

            // Update the rolling average FPS
            if (delta.count() > 0.0) {
                double fps = 1.0 / delta.count();

                if (fpsHistory.size() >= maxHistorySize) {
                    fpsHistory.pop_front();
                }
                fpsHistory.push_back(fps);
            }
        }

        // Returns the frametime of a prior frame
        double getFrameTime(size_t framesAgo = 1) const {
            if (framesAgo == 0 || framesAgo > frameTimes.size()) {
                throw std::out_of_range("framesAgo out of range");
            }
            size_t index = (frameIndex + frameTimes.size() - framesAgo) % frameTimes.size();
            return frameTimes[index];
        }

        // Returns the average FPS over the history
        double getAverageFPS() const {
            if (fpsHistory.empty()) return 0.0;
            double sum = 0.0;
            for (double fps : fpsHistory) {
                sum += fps;
            }
            return sum / fpsHistory.size();
        }

        std::deque<double> getFPSHistory(size_t length = maxHistorySize) const {
            return fpsHistory;
        }

        double getDelta() const {
            return delta.count();
        }

    private:
        std::chrono::time_point<std::chrono::high_resolution_clock> startTime;
        std::chrono::time_point<std::chrono::high_resolution_clock> drawStartTime;

        std::array<double, 5> frameTimes = {}; // Circular buffer for frametimes
        size_t frameIndex = 0;                 // Index for the current frame
        std::deque<double> fpsHistory;        // Rolling FPS history
        static constexpr size_t maxHistorySize = 100; // Adjustable history size
        std::chrono::duration<double> delta;
};

} //namespace