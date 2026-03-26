#include "Renderer/Scene/RenderExtractionManager.hpp"

#include "Renderer/Scene/Extractors/CameraRenderExtractor.hpp"
#include "Renderer/Scene/Extractors/MeshRenderExtractor.hpp"
#include "Renderer/Scene/RenderExtractionContext.hpp"

namespace Faye
{
    RenderExtractionManager::RenderExtractionManager()
    {
        extractors.push_back(std::make_unique<CameraRenderExtractor>());
        extractors.push_back(std::make_unique<MeshRenderExtractor>());
    }

    RenderSceneSnapshot RenderExtractionManager::extract(const Scene &scene, ModelRegistry &modelRegistry) const
    {
        RenderSceneSnapshot snapshot{};
        RenderExtractionContext context{scene, modelRegistry, snapshot};

        for (const auto &extractor : extractors)
        {
            extractor->extract(context);
        }

        return snapshot;
    }
}