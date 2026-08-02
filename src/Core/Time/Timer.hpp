#pragma once

#include <ctime>
#include <chrono>

#include "Core/ITick.hpp"

namespace Faye::Time
{
    class Timer : public ITick
    {
    public:
        void OnInit() override;
        void OnUpdate(const EngineContext&) override;

        double getTimeMS() const { return time_ms; }
        double getDeltaTimeMS() const { return delta_time_ms; }
        double getTimeS() const { return time_ms / 1000.0; }
        double getDeltaTimeS() const { return delta_time_ms / 1000.0; }

    private:
        std::chrono::high_resolution_clock::time_point startPoint;
        std::chrono::high_resolution_clock::time_point endPoint;

        double time_ms = 0.0f;
        double delta_time_ms = 0.0f;
    };

    struct StopWatch
    {
    public:
        StopWatch() { start(); }
        void start() { startTime = std::chrono::high_resolution_clock::now(); }
        double elapsedMs() const;

    private:
        std::chrono::high_resolution_clock::time_point startTime;
    };
} // namespace Faye::Time