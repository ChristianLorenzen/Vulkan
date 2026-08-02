#pragma once

#include "Core/ECS/System.hpp"
#include "Renderer/Scene/RenderScene.hpp"

namespace Faye
{
    // Read-only extraction of directional lights into the snapshot. Runs
    // concurrently with the mesh + point-light extractors: all only READ
    // TransformComponent (readers never conflict) and write disjoint snapshot
    // fields.
    class DirectionalLightRenderExtractor final : public Ecs::ISystem
    {
    public:
        explicit DirectionalLightRenderExtractor(RenderSceneSnapshot &snapshot) : snapshot(snapshot) {}

        const char *name() const override { return "DirectionalLightExtract"; }
        Ecs::SystemAccess access() const override;
        void run(Ecs::World &world, const EngineContext &ctx,
                 Jobs::JobSystem &jobs, Ecs::CommandBuffer &commands) override;

    private:
        RenderSceneSnapshot &snapshot;
    };
}
