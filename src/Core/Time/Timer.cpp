#include "Timer.hpp"
#include "Core/Logging/Logger.hpp"

namespace Faye
{
    void Time::Timer::OnInit()
    {
        startPoint = std::chrono::high_resolution_clock::now();
        endPoint = startPoint;
        LOG_INFO(Logger::get(), "Timer initialized");
    }

    void Time::Timer::OnUpdate(const EngineContext&)
    {
        startPoint = std::chrono::high_resolution_clock::now();

        delta_time_ms = std::chrono::duration<double, std::milli>(startPoint - endPoint).count();
        time_ms += delta_time_ms;

        endPoint = startPoint;
    }

    double Time::StopWatch::elapsedMs() const {
        auto now = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::milli>(now - startTime).count();
    }
}