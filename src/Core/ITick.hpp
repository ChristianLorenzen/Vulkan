#pragma once
#include "EngineContext.hpp"

namespace Faye
{
    class ITick
    {
    public:
        virtual ~ITick() = default;
        virtual void OnInit() {}
        virtual void OnPostInit() {}
        virtual void OnUpdate(const EngineContext&) {}
        // Fixed timestep update, physics, etc.
        virtual void OnFixedUpdate(const EngineContext&) {}
        virtual void OnStop() {}
    };
}