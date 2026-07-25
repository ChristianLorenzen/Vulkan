#pragma once

#include "Core/ECS/System.hpp"
#include "Renderer/Scene/RenderScene.hpp"

namespace Faye
{
    // Read-only extraction of point lights into the snapshot. Runs concurrently
    // with the mesh extractor: both only READ TransformComponent (readers never
    // conflict) and write disjoint snapshot fields.
    class PointLightRenderExtractor final : public Ecs::ISystem
    {
    public:
        explicit PointLightRenderExtractor(RenderSceneSnapshot &snapshot) : snapshot(snapshot) {}

        const char *name() const override { return "PointLightExtract"; }
        Ecs::SystemAccess access() const override;
        void run(Ecs::World &world, const EngineContext &ctx,
                 Jobs::JobSystem &jobs, Ecs::CommandBuffer &commands) override;

    private:
        RenderSceneSnapshot &snapshot;
    };
}
