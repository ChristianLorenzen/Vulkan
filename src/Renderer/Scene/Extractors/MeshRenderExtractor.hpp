#pragma once

#include <cstdint>

#include "Assets/ModelRegistry.hpp"
#include "Core/ECS/System.hpp"
#include "Renderer/Material/MaterialRegistry.hpp"
#include "Renderer/Scene/RenderScene.hpp"

namespace Faye
{
    // Read-only extraction of renderable meshes into the snapshot. Declared as
    // an ISystem so the SystemSchedule runs it concurrently with the other
    // read-only extractors (they touch only overlapping READS and write disjoint
    // snapshot fields, so the conflict DAG proves them independent).
    class MeshRenderExtractor final : public Ecs::ISystem
    {
    public:
        MeshRenderExtractor(ModelRegistry &models, MaterialRegistry &materials,
                            RenderSceneSnapshot &snapshot, const uint64_t &extractionIndex)
            : models(models), materials(materials), snapshot(snapshot), extractionIndex(extractionIndex) {}

        const char *name() const override { return "MeshExtract"; }
        Ecs::SystemAccess access() const override;
        void run(Ecs::World &world, const EngineContext &ctx,
                 Jobs::JobSystem &jobs, Ecs::CommandBuffer &commands) override;

    private:
        ModelRegistry &models;
        MaterialRegistry &materials;
        RenderSceneSnapshot &snapshot;
        const uint64_t &extractionIndex;
    };
}
