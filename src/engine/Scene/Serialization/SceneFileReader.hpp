#pragma once

#include <string>

#include "engine/Scene/Entities/Entity.hpp"
namespace Faye
{
    class Scene;
    class ModelRegistry;
    class MaterialRegistry;
    class AssetDatabase;
    class ScriptSystem;
    class LuaScriptSystem;

    struct SceneFileLoadResult
    {
        bool success = false;
        std::string error{};
        Entity activeCamera;          // primary camera entity (invalid if none)
        Entity postProcessSettings;   // post-process entity (invalid if none)
    };

    // Parses a .faye YAML scene into an already-created Scene. Assets are
    // imported/created first so mesh component asset references resolve; then
    // entities are created with their persisted GUIDs and components filled
    // through the ComponentTypeRegistry deserialize slots. Scripts are
    // re-attached by path afterwards. See docs/serialization/schemas.md.
    class SceneFileReader
    {
    public:
        static constexpr int kSceneSchemaVersion = 1;

        static SceneFileLoadResult read(Scene &scene,
                                        const std::string &yamlText,
                                        ModelRegistry &models,
                                        MaterialRegistry &materials,
                                        AssetDatabase &assets,
                                        ScriptSystem &scripts,
                                        LuaScriptSystem &luaScripts);
    };
}
