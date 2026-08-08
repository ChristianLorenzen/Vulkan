#pragma once

#include <array>
#include <cstddef>
#include <string>
#include <filesystem>

#include "engine/Assets/AssetDatabase.hpp"
#include "engine/Assets/ModelRegistry.hpp"
#include "Core/Handles/PrimitiveType.hpp"
#include "Renderer/Material/MaterialRegistry.hpp"
#include "Renderer/Resources/Model.hpp"
#include "engine/Scene/Scene.hpp"
#include "engine/Scene/Entities/Entity.hpp"
#include "Core/Jobs/JobSystem.hpp"

namespace Faye
{
    class ScriptSystem;
    class LuaScriptSystem;

    /// Composes scene content out of the asset registries and scripting engines.
    ///
    /// This is the one type that legitimately depends on "everything", because
    /// its whole job is to assemble entities from models, materials and scripts.
    /// Localising that here keeps SceneManager and the registries free of the
    /// cross-dependency web — they stay leaves; this is the coordinator.
    class SceneBuilder
    {
    public:
        struct SceneSetup
        {
            Entity activeCamera;
            Entity postProcessSettings;
        };

        SceneBuilder(ModelRegistry &models,
                     MaterialRegistry &materials,
                     AssetDatabase &assetDatabase,
                     ScriptSystem &scripts,
                     LuaScriptSystem &luaScripts,
                     Jobs::JobSystem &jobSystem
                    );

        /// Populate the already-created, empty scene with the demo content and
        /// return the entities the engine needs to drive the frame. Assumes the
        /// scripting systems have already bound this scene (OnPostInit).
        SceneSetup populate(Scene &scene);

        void populateDefaultScene(Scene &scene);

        Entity importAndCreateModel(Scene &scene, std::filesystem::path path);

        /// Create a primitive entity at runtime — the editor "add primitive"
        /// action routes here.
        Entity createPrimitiveEntity(Scene &scene, PrimitiveType primitiveType);

    private:
        struct ImportedModelRegistration
        {
            ModelHandle handle{};
            Model::Bounds bounds{};
        };

        ModelHandle ensurePrimitiveHandle(PrimitiveType primitiveType);
        ImportedModelRegistration registerImportedModelWithBounds(const std::string &modelPath,
                                                                   MaterialPipelineConfig pipelineConfig = {});
        // Lazily provides the water material: reuses the already-registered
        // built-in (e.g. one loaded from a scene file), else builds + registers
        // the default water material (alpha-blended, water domain, real maps).
        // Safe to call when the scene was loaded from disk (populate() never
        // ran, so waterMaterialHandle is unset).
        MaterialHandle ensureWaterMaterial();

        static constexpr std::size_t primitiveIndex(PrimitiveType primitiveType)
        {
            return static_cast<std::size_t>(primitiveType);
        }

        ModelRegistry &models;
        MaterialRegistry &materials;
        AssetDatabase &assetDatabase;
        ScriptSystem &scripts;
        LuaScriptSystem &luaScripts;
        Jobs::JobSystem &jobSystem;

        std::array<ModelHandle, static_cast<std::size_t>(PrimitiveType::Count)> primitiveModelHandles{};
        MaterialHandle waterMaterialHandle{};
    };
}
