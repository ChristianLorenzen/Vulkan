#pragma once

#include <string>

namespace Faye
{
    class Scene;
    class ModelRegistry;
    class MaterialRegistry;
    class AssetDatabase;

    // Serializes a live scene to the .faye YAML format (see
    // docs/serialization/schemas.md). Entity/component data flows through the
    // ComponentTypeRegistry serialize slots; asset records come from the
    // AssetDatabase (persisted records only), with material data + pipeline
    // inlined from the MaterialRegistry.
    class SceneFileWriter
    {
    public:
        static constexpr int kSceneSchemaVersion = 1;

        static std::string write(Scene &scene,
                                 const ModelRegistry &models,
                                 const MaterialRegistry &materials,
                                 const AssetDatabase &assets);
    };
}
