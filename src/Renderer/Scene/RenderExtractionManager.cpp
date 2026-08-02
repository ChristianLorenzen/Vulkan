#include "Renderer/Scene/RenderExtractionManager.hpp"

#include <memory>

#include "Core/ECS/World.hpp"
#include "Renderer/Scene/Extractors/DirectionalLightRenderExtractor.hpp"
#include "Renderer/Scene/Extractors/MeshRenderExtractor.hpp"
#include "Renderer/Scene/Extractors/PointLightRenderExtractor.hpp"

namespace Faye
{
    RenderExtractionManager::RenderExtractionManager(ModelRegistry &models, MaterialRegistry &materials)
    {
        // The extractors hold references into this manager's persistent state
        // (snapshot + extractionIndex) and the asset registries; the schedule
        // runs them in parallel each frame.
        extractSchedule.addSystem(Ecs::Stage::Extract,
                                  std::make_unique<MeshRenderExtractor>(models, materials, snapshot, extractionIndex));
        extractSchedule.addSystem(Ecs::Stage::Extract,
                                  std::make_unique<PointLightRenderExtractor>(snapshot));
        extractSchedule.addSystem(Ecs::Stage::Extract,
                                  std::make_unique<DirectionalLightRenderExtractor>(snapshot));
    }

    const RenderSceneSnapshot &RenderExtractionManager::extract(Scene &scene, Jobs::JobSystem &jobs)
    {
        // Reuse the persistent buffers: clear() keeps capacity, so steady-state
        // extraction performs no heap allocation.
        snapshot.primaryCamera = nullptr;
        snapshot.renderables.clear();
        snapshot.pointLights.clear();
        snapshot.directionalLights.clear();
        ++extractionIndex;

        // Camera: needs the Scene's primary-camera bookkeeping (not a plain
        // view), so it is resolved here rather than as a scheduled system.
        if (const CameraComponent *camera = scene.getPrimaryCamera())
        {
            snapshot.primaryCamera = &camera->camera;
        }

        // Mesh + point-light extraction: read-only view passes writing disjoint
        // snapshot fields — the schedule runs them concurrently.
        EngineContext ctx{};
        ctx.jobs = &jobs;
        extractSchedule.runStage(Ecs::Stage::Extract, scene.getWorld(), ctx, jobs);

        // Motion-vector write-back: single-threaded, after the read barrier, so
        // this structural change (add PreviousTransformComponent) cannot race a
        // view iteration. World::destroy prunes the component with the entity.
        Ecs::World &world = scene.getWorld();
        for (const auto &renderable : snapshot.renderables)
        {
            PreviousTransformComponent *history = world.tryGet<PreviousTransformComponent>(renderable.entity);
            if (history == nullptr)
            {
                history = &world.add<PreviousTransformComponent>(renderable.entity);
            }
            history->modelMatrix = renderable.modelMatrix;
            history->lastSeenExtraction = extractionIndex;
        }

        return snapshot;
    }
}
