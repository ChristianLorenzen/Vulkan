#pragma once

#include <algorithm>

namespace Faye
{
    // Standard fixed-timestep accumulator. advance() returns how many fixed
    // ticks to run for this rendered frame; fractional leftover time carries in
    // `accumulator`, so fixed-step work runs at exactly 1/step Hz regardless of
    // render rate (144 fps => a tick every ~2.4 frames; 30 fps => 2 per frame).
    class FixedStepper
    {
    public:
        explicit FixedStepper(double stepSeconds) : step(stepSeconds) {}

        int advance(double dtSeconds)
        {
            // Clamp: a debugger pause or hitch must not schedule a 10-second
            // catch-up spiral (each catch-up tick stalls the frame, which grows
            // the accumulator, which...).
            accumulator += std::min(dtSeconds, kMaxFrameTime);
            int ticks = 0;
            while (accumulator >= step)
            {
                accumulator -= step;
                ++ticks;
            }
            return ticks;
        }

        double stepSeconds() const { return step; }

    private:
        static constexpr double kMaxFrameTime = 0.25;
        double accumulator = 0.0;
        double step;
    };
}
