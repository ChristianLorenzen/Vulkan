#include "Renderer/Scene/RenderExtractionManager.hpp"

#include "Renderer/Scene/Extractors/CameraRenderExtractor.hpp"
#include "Renderer/Scene/Extractors/MeshRenderExtractor.hpp"
#include "Renderer/Scene/Extractors/PointLightRenderExtractor.hpp"
#include "Renderer/Scene/RenderExtractionContext.hpp"

namespace Faye
{
    RenderExtractionManager::RenderExtractionManager()
    {
        extractors.push_back(std::make_unique<CameraRenderExtractor>());
        extractors.push_back(std::make_unique<MeshRenderExtractor>());
        extractors.push_back(std::make_unique<PointLightRenderExtractor>());
    }

    RenderSceneSnapshot RenderExtractionManager::extract(const Scene &scene, ModelRegistry &modelRegistry, MaterialRegistry &materialRegistry)
    {
        RenderSceneSnapshot snapshot{};
        const uint64_t currentExtractionIndex = ++extractionIndex;
        RenderExtractionContext context{scene, modelRegistry, materialRegistry, snapshot, previousModelTransforms, currentExtractionIndex};

        for (const auto &extractor : extractors)
        {
            extractor->extract(context);
        }

        // Update the previous model transforms for all renderables in this scene.
        // This is used for motion vectors
        for (const auto &renderable : snapshot.renderables)
        {
            previousModelTransforms[renderable.entity] = RenderTransformHistoryEntry{
                renderable.modelMatrix,
                currentExtractionIndex};
        }

        return snapshot;
    }
}