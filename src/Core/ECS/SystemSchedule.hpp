#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

#include "Core/ECS/CommandBuffer.hpp"
#include "Core/ECS/System.hpp"
#include "Core/Jobs/JobSystem.hpp"

namespace Faye::Ecs
{
    enum class Stage : uint8_t
    {
        Input,
        FixedUpdate,   // run 0..N times per frame by the fixed-step accumulator
        Update,
        Extract,       // CPU snapshot for the renderer
        Render,        // main-thread heavy by nature
        Count,
    };

    // Ordered stages with a hard barrier between them; within a stage a
    // dependency DAG is built from access-set conflicts, so non-conflicting
    // systems run concurrently while conflicting ones keep registration order.
    class SystemSchedule
    {
    public:
        void addSystem(Stage stage, std::unique_ptr<ISystem> system);
        void runStage(Stage stage, World &world, const EngineContext &ctx,
                      Jobs::JobSystem &jobs);

    private:
        struct ScheduledSystem
        {
            std::unique_ptr<ISystem> system;
            CompiledAccess access;
            // Indices of earlier same-stage systems whose access conflicts with
            // ours. Edges always point earlier -> later, so the graph is acyclic
            // BY CONSTRUCTION — no cycle detection needed.
            std::vector<uint32_t> dependencies;
        };

        std::array<std::vector<ScheduledSystem>, size_t(Stage::Count)> stages;
        std::vector<CommandBuffer> commandBuffers;   // persistent; reused each stage
    };
}
