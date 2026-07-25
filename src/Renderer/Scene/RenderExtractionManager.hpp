#pragma once

#include <cstdint>

#include "Assets/ModelRegistry.hpp"
#include "Core/ECS/SystemSchedule.hpp"
#include "Core/Jobs/JobSystem.hpp"
#include "Renderer/Material/MaterialRegistry.hpp"
#include "Renderer/Scene/RenderScene.hpp"
#include "Scene/Scene.hpp"

namespace Faye
{
    // Builds the per-frame render snapshot. The read-only mesh + point-light
    // extractors are ISystems in an Extract stage, so the SystemSchedule runs
    // them concurrently on the job system (the conflict DAG proves them
    // independent). Camera selection (needs Scene-level primary-camera state) and
    // the motion-vector write-back (a structural change) stay single-threaded.
    class RenderExtractionManager
    {
    public:
        RenderExtractionManager(ModelRegistry &models, MaterialRegistry &materials);

        // Returns a reference into the persistent snapshot: valid until the next
        // extract() call. Callers consume it within the frame.
        const RenderSceneSnapshot &extract(Scene &scene, Jobs::JobSystem &jobs);

    private:
        RenderSceneSnapshot snapshot;   // persistent: cleared + refilled each frame
        uint64_t extractionIndex = 0;
        Ecs::SystemSchedule extractSchedule;   // Stage::Extract: the parallel extractors
    };
}
