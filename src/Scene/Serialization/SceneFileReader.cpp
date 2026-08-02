#include "Scene/Serialization/SceneFileReader.hpp"

#include <yaml-cpp/yaml.h>

#include "Assets/AssetDatabase.hpp"
#include "Assets/ModelRegistry.hpp"
#include "Core/Logging/Logger.hpp"
#include "Renderer/Material/MaterialRegistry.hpp"
#include "Renderer/Material/TextureLoader.hpp"
#include "Renderer/Resources/PrimitiveType.hpp"
#include "Scene/Scene.hpp"
#include "Scene/Serialization/Deserializer.hpp"
#include "Scene/Serialization/MaterialEnumNames.hpp"
#include "Scripting/LuaScriptSystem.hpp"
#include "Scripting/ScriptSystem.hpp"
#include "quill/LogMacros.h"

namespace Faye
{
    namespace
    {
        const Ecs::ComponentTypeInfo *findComponentByName(const Ecs::World &world, const std::string &name)
        {
            for (const Ecs::ComponentTypeInfo &info : world.types().all())
            {
                if (info.name != nullptr && name == info.name)
                    return &info;
            }
            return nullptr;
        }

        MaterialData readMaterialData(const YAML::Node &node)
        {
            MaterialData data;
            data.name = Ecs::Deserializer::readStringFrom(node, "name", data.name);
            data.color = Ecs::Deserializer::readVec3From(node, "color", data.color);
            data.baseColorFactor = Ecs::Deserializer::readVec4From(node, "baseColorFactor", data.baseColorFactor);
            data.diffuse = Ecs::Deserializer::readVec3From(node, "diffuse", data.diffuse);
            data.ambient = Ecs::Deserializer::readVec3From(node, "ambient", data.ambient);
            data.specular = Ecs::Deserializer::readVec3From(node, "specular", data.specular);
            data.emissive = Ecs::Deserializer::readVec3From(node, "emissive", data.emissive);
            data.shininess = Ecs::Deserializer::readFloatFrom(node, "shininess", data.shininess);
            data.opacity = Ecs::Deserializer::readFloatFrom(node, "opacity", data.opacity);
            data.metallicFactor = Ecs::Deserializer::readFloatFrom(node, "metallicFactor", data.metallicFactor);
            data.roughnessFactor = Ecs::Deserializer::readFloatFrom(node, "roughnessFactor", data.roughnessFactor);
            data.normalScale = Ecs::Deserializer::readFloatFrom(node, "normalScale", data.normalScale);
            data.occlusionStrength = Ecs::Deserializer::readFloatFrom(node, "occlusionStrength", data.occlusionStrength);
            data.specularStrength = Ecs::Deserializer::readFloatFrom(node, "specularStrength", data.specularStrength);
            data.reflectivity = Ecs::Deserializer::readFloatFrom(node, "reflectivity", data.reflectivity);
            data.emissiveIntensity = Ecs::Deserializer::readFloatFrom(node, "emissiveIntensity", data.emissiveIntensity);
            data.alphaCutoff = Ecs::Deserializer::readFloatFrom(node, "alphaCutoff", data.alphaCutoff);
            data.doubleSided = Ecs::Deserializer::readBoolFrom(node, "doubleSided", data.doubleSided);
            if (const auto mode = materialAlphaModeFromName(Ecs::Deserializer::readStringFrom(node, "alphaMode", "")))
                data.alphaMode = *mode;

            const YAML::Node textures = node["textures"];
            if (textures && textures.IsSequence())
            {
                for (const YAML::Node &textureNode : textures)
                {
                    if (!textureNode.IsMap())
                        continue;
                    const std::string path = Ecs::Deserializer::readStringFrom(textureNode, "path", "");
                    const std::string typeName = Ecs::Deserializer::readStringFrom(textureNode, "type", "");
                    const auto type = textureTypeFromName(typeName);
                    if (path.empty() || !type)
                        continue;

                    // Decode + embed the pixels so the GPU texture cache uploads
                    // the real image. A path-only reference (no pixel data) is
                    // treated as invalid by the cache and renders as the 1x1
                    // per-type fallback.
                    try
                    {
                        data.textures.push_back(loadTextureFromFile(path, *type));
                    }
                    catch (const std::exception &e)
                    {
                        LOG_WARNING(Logger::get(), "Scene load: failed to load texture '{}' ({}); using fallback.",
                                    path, e.what());
                        data.textures.push_back(Texture::create(path, *type));
                    }
                }
            }
            return data;
        }

        MaterialPipelineConfig readMaterialPipeline(const YAML::Node &node)
        {
            MaterialPipelineConfig config;
            config.vertexShaderPath = Ecs::Deserializer::readStringFrom(node, "vertexShaderPath", config.vertexShaderPath);
            config.fragmentShaderPath = Ecs::Deserializer::readStringFrom(node, "fragmentShaderPath", config.fragmentShaderPath);
            config.enableAlphaBlending = Ecs::Deserializer::readBoolFrom(node, "enableAlphaBlending", config.enableAlphaBlending);
            config.tessControlShaderPath = Ecs::Deserializer::readStringFrom(node, "tessControlShaderPath", config.tessControlShaderPath);
            config.tessEvalShaderPath = Ecs::Deserializer::readStringFrom(node, "tessEvalShaderPath", config.tessEvalShaderPath);
            if (const auto domain = materialDomainFromName(Ecs::Deserializer::readStringFrom(node, "domain", "")))
                config.domain = *domain;
            return config;
        }

        // Re-attach scripts that were persisted as path-only components. The
        // component deserialize slot fills the path; the runtime state (sol2
        // env / dlopen handle) is rebuilt here, after the entity exists.
        // NOTE: the path is COPIED to a local first — loadScript() begins by
        // unloading any existing script, which removes the very component we
        // are reading, so a reference into it would dangle.
        void reattachScripts(Scene &scene, Entity entity, ScriptSystem &scripts, LuaScriptSystem &luaScripts)
        {
            if (auto *lua = entity.tryGet<LuaScriptComponent>())
            {
                const std::string scriptPath = lua->scriptPath;   // copy (see note)
                if (!scriptPath.empty())
                {
                    try
                    {
                        luaScripts.loadScript(entity, scriptPath, &scene);
                    }
                    catch (const std::exception &e)
                    {
                        LOG_WARNING(Logger::get(), "Scene load: failed to re-attach Lua script '{}': {}",
                                    scriptPath, e.what());
                    }
                }
            }
            if (auto *native = entity.tryGet<NativeScriptComponent>())
            {
                const std::string scriptPath = native->scriptPath;   // copy (see note)
                if (!scriptPath.empty() && scriptPath != "<builtin>")
                {
                    try
                    {
                        scripts.loadScript(entity, scriptPath);
                    }
                    catch (const std::exception &e)
                    {
                        LOG_WARNING(Logger::get(), "Scene load: failed to re-attach native script '{}': {}",
                                    scriptPath, e.what());
                    }
                }
            }
        }
    }

    SceneFileLoadResult SceneFileReader::read(Scene &scene,
                                              const std::string &yamlText,
                                              ModelRegistry &models,
                                              MaterialRegistry &materials,
                                              AssetDatabase &assets,
                                              ScriptSystem &scripts,
                                              LuaScriptSystem &luaScripts)
    {
        SceneFileLoadResult result;

        YAML::Node root;
        try
        {
            root = YAML::Load(yamlText);
        }
        catch (const YAML::Exception &e)
        {
            result.error = "YAML parse error: " + std::string(e.what());
            return result;
        }

        if (!root.IsMap())
        {
            result.error = "Scene file must be a YAML map";
            return result;
        }

        const YAML::Node versionNode = root["schemaVersion"];
        if (!versionNode || !versionNode.IsScalar() ||
            versionNode.as<int>(0) != SceneFileReader::kSceneSchemaVersion)
        {
            result.error = "Unsupported schemaVersion (expected " +
                           std::to_string(SceneFileReader::kSceneSchemaVersion) + ")";
            return result;
        }

        // --- scene meta ----------------------------------------------------
        const YAML::Node sceneNode = root["scene"];
        if (sceneNode && sceneNode.IsMap())
        {
            try
            {
                if (sceneNode["uuid"] && sceneNode["uuid"].IsScalar())
                    scene.setSceneUuid(Uuid::fromString(sceneNode["uuid"].as<std::string>()));
            }
            catch (const std::exception &)
            {
            }
            scene.setName(Ecs::Deserializer::readStringFrom(sceneNode, "name", std::string(scene.getName())));
        }

        // --- assets first so mesh component references resolve --------------
        const YAML::Node assetsNode = root["assets"];
        if (assetsNode && assetsNode.IsSequence())
        {
            for (const YAML::Node &recordNode : assetsNode)
            {
                if (!recordNode.IsMap())
                    continue;
                const std::string idText = Ecs::Deserializer::readStringFrom(recordNode, "id", "");
                if (idText.empty())
                    continue;
                Uuid id;
                try
                {
                    id = Uuid::fromString(idText);
                }
                catch (const std::exception &)
                {
                    LOG_WARNING(Logger::get(), "Scene load: skipping asset with invalid id '{}'", idText);
                    continue;
                }

                const std::string typeText = Ecs::Deserializer::readStringFrom(recordNode, "type", "");
                if (typeText == "model")
                {
                    // Deterministic ids: createPrimitive/getOrImportByUri
                    // derive the same id the writer emitted.
                    const std::string primitive = Ecs::Deserializer::readStringFrom(recordNode, "primitive", "");
                    const std::string uri = Ecs::Deserializer::readStringFrom(recordNode, "uri", "");
                    if (!primitive.empty())
                    {
                        if (const auto primitiveType = primitiveTypeFromName(primitive))
                            models.createPrimitive(*primitiveType);
                        else
                            LOG_WARNING(Logger::get(), "Scene load: unknown primitive '{}'", primitive);
                    }
                    else if (!uri.empty())
                    {
                        try
                        {
                            models.getOrImportByUri(uri);
                        }
                        catch (const std::exception &e)
                        {
                            LOG_WARNING(Logger::get(), "Scene load: failed to import model '{}': {}", uri, e.what());
                        }
                    }
                }
                else if (typeText == "material")
                {
                    const YAML::Node dataNode = recordNode["data"];
                    const YAML::Node pipelineNode = recordNode["pipeline"];
                    MaterialData data = dataNode && dataNode.IsMap() ? readMaterialData(dataNode) : MaterialData{};
                    MaterialPipelineConfig config = pipelineNode && pipelineNode.IsMap() ? readMaterialPipeline(pipelineNode) : MaterialPipelineConfig{};
                    materials.registerMaterial(std::move(data), std::move(config), id);
                    assets.registerAsset(AssetRecord{
                        .id = id,
                        .type = AssetType::Material,
                        .name = Ecs::Deserializer::readStringFrom(recordNode, "name", ""),
                        .persistence = AssetPersistenceMode::Imported,
                    });
                }
            }
        }

        // --- entities ------------------------------------------------------
        const YAML::Node entitiesNode = root["entities"];
        if (!entitiesNode || !entitiesNode.IsSequence())
        {
            result.error = "Scene file has no entities";
            return result;
        }

        Ecs::World &world = scene.getWorld();
        Entity primaryCamera;
        Entity postProcessSettings;

        for (const YAML::Node &entityNode : entitiesNode)
        {
            if (!entityNode.IsMap())
                continue;
            const std::string idText = Ecs::Deserializer::readStringFrom(entityNode, "id", "");
            if (idText.empty())
            {
                result.error = "Entity missing id";
                return result;
            }
            Uuid id;
            try
            {
                id = Uuid::fromString(idText);
            }
            catch (const std::exception &)
            {
                result.error = "Entity has invalid id '" + idText + "'";
                return result;
            }

            Entity entity = scene.createEntityWithGuid(
                Ecs::Deserializer::readStringFrom(entityNode, "name", ""), id);

            const YAML::Node compsNode = entityNode["components"];
            if (compsNode && compsNode.IsSequence())
            {
                for (const YAML::Node &compNode : compsNode)
                {
                    if (!compNode.IsMap())
                        continue;
                    const std::string typeName = Ecs::Deserializer::readStringFrom(compNode, "type", "");
                    if (typeName.empty())
                        continue;

                    const Ecs::ComponentTypeInfo *info = findComponentByName(world, typeName);
                    if (info == nullptr)
                    {
                        LOG_WARNING(Logger::get(), "Scene load: unknown component type '{}' (skipped)", typeName);
                        continue;   // forward compatibility
                    }
                    if (info->deserialize == nullptr)
                    {
                        LOG_WARNING(Logger::get(), "Scene load: component '{}' is not deserializable (skipped)", typeName);
                        continue;
                    }

                    info->addDefault(world, entity.handle());
                    void *raw = info->tryGetRaw(world, entity.handle());
                    if (raw == nullptr)
                        continue;
                    Ecs::Deserializer deserializer(compNode, &models, &materials);
                    info->deserialize(raw, deserializer);
                }
            }

            reattachScripts(scene, entity, scripts, luaScripts);

            if (auto *camera = entity.tryGet<CameraComponent>())
            {
                if (camera->primary)
                    primaryCamera = entity;
            }
            if (entity.tryGet<PostProcessStackComponent>() != nullptr && !postProcessSettings.isValid())
                postProcessSettings = entity;
        }

        // --- hard requirements: exactly one primary camera + post process ---
        if (primaryCamera.isValid())
        {
            scene.setPrimaryCamera(primaryCamera);
            result.activeCamera = primaryCamera;
        }
        else
        {
            result.error = "Loaded scene has no primary camera (Transform + Camera, primary: true)";
        }
        result.postProcessSettings = postProcessSettings;

        result.success = true;
        return result;
    }
}
