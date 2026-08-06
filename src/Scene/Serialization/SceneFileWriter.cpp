#include "Scene/Serialization/SceneFileWriter.hpp"

#include <yaml-cpp/yaml.h>

#include "Assets/AssetDatabase.hpp"
#include "Assets/ModelRegistry.hpp"
#include "Renderer/Material/MaterialRegistry.hpp"
#include "Scene/Scene.hpp"
#include "Scene/Serialization/MaterialEnumNames.hpp"
#include "Scene/Serialization/ReflectedSerializers.hpp"
#include "Scene/Serialization/Serializer.hpp"

namespace Faye
{
    namespace
    {
        void writeMaterialData(YAML::Emitter &emitter, const MaterialData &data)
        {
            emitter << YAML::Key << "data" << YAML::Value << YAML::BeginMap;
            emitter << YAML::Key << "name" << YAML::Value << data.name;
            emitter << YAML::Key << "color" << YAML::Value << YAML::Flow << YAML::BeginSeq
                    << data.color.x << data.color.y << data.color.z << YAML::EndSeq;
            emitter << YAML::Key << "baseColorFactor" << YAML::Value << YAML::Flow << YAML::BeginSeq
                    << data.baseColorFactor.x << data.baseColorFactor.y << data.baseColorFactor.z << data.baseColorFactor.w
                    << YAML::EndSeq;
            emitter << YAML::Key << "diffuse" << YAML::Value << YAML::Flow << YAML::BeginSeq
                    << data.diffuse.x << data.diffuse.y << data.diffuse.z << YAML::EndSeq;
            emitter << YAML::Key << "ambient" << YAML::Value << YAML::Flow << YAML::BeginSeq
                    << data.ambient.x << data.ambient.y << data.ambient.z << YAML::EndSeq;
            emitter << YAML::Key << "specular" << YAML::Value << YAML::Flow << YAML::BeginSeq
                    << data.specular.x << data.specular.y << data.specular.z << YAML::EndSeq;
            emitter << YAML::Key << "emissive" << YAML::Value << YAML::Flow << YAML::BeginSeq
                    << data.emissive.x << data.emissive.y << data.emissive.z << YAML::EndSeq;
            emitter << YAML::Key << "shininess" << YAML::Value << data.shininess;
            emitter << YAML::Key << "opacity" << YAML::Value << data.opacity;
            emitter << YAML::Key << "metallicFactor" << YAML::Value << data.metallicFactor;
            emitter << YAML::Key << "roughnessFactor" << YAML::Value << data.roughnessFactor;
            emitter << YAML::Key << "normalScale" << YAML::Value << data.normalScale;
            emitter << YAML::Key << "occlusionStrength" << YAML::Value << data.occlusionStrength;
            emitter << YAML::Key << "specularStrength" << YAML::Value << data.specularStrength;
            emitter << YAML::Key << "reflectivity" << YAML::Value << data.reflectivity;
            emitter << YAML::Key << "emissiveIntensity" << YAML::Value << data.emissiveIntensity;
            emitter << YAML::Key << "alphaMode" << YAML::Value << materialAlphaModeName(data.alphaMode);
            emitter << YAML::Key << "alphaCutoff" << YAML::Value << data.alphaCutoff;
            emitter << YAML::Key << "doubleSided" << YAML::Value << data.doubleSided;

            emitter << YAML::Key << "textures" << YAML::Value << YAML::BeginSeq;
            for (const Texture &texture : data.textures)
            {
                emitter << YAML::BeginMap;
                emitter << YAML::Key << "type" << YAML::Value << textureTypeName(texture.type);
                emitter << YAML::Key << "path" << YAML::Value << texture.path;
                emitter << YAML::EndMap;
            }
            emitter << YAML::EndSeq;
            emitter << YAML::EndMap;
        }

        void writeMaterialPipeline(YAML::Emitter &emitter, const MaterialPipelineConfig &config)
        {
            emitter << YAML::Key << "pipeline" << YAML::Value << YAML::BeginMap;
            emitter << YAML::Key << "vertexShaderPath" << YAML::Value << config.vertexShaderPath;
            emitter << YAML::Key << "fragmentShaderPath" << YAML::Value << config.fragmentShaderPath;
            emitter << YAML::Key << "enableAlphaBlending" << YAML::Value << config.enableAlphaBlending;
            emitter << YAML::Key << "domain" << YAML::Value << materialDomainName(config.domain);
            emitter << YAML::Key << "tessControlShaderPath" << YAML::Value << config.tessControlShaderPath;
            emitter << YAML::Key << "tessEvalShaderPath" << YAML::Value << config.tessEvalShaderPath;
            emitter << YAML::EndMap;
        }
    }

    std::string SceneFileWriter::write(Scene &scene,
                                       const ModelRegistry &models,
                                       const MaterialRegistry &materials,
                                       const AssetDatabase &assets)
    {
        YAML::Emitter emitter;
        emitter << YAML::BeginMap;

        emitter << YAML::Key << "schemaVersion" << YAML::Value << kSceneSchemaVersion;

        emitter << YAML::Key << "scene" << YAML::Value << YAML::BeginMap;
        emitter << YAML::Key << "uuid" << YAML::Value << scene.getSceneUuid().toString();
        emitter << YAML::Key << "name" << YAML::Value << std::string(scene.getName());
        emitter << YAML::EndMap;

        // --- assets: persisted records (built-in/imported), material data inlined ---
        emitter << YAML::Key << "assets" << YAML::Value << YAML::BeginSeq;
        for (const AssetId &id : assets.getAllAssetIds())
        {
            const AssetRecord *record = assets.findByAssetId(id);
            if (record == nullptr || record->persistence == AssetPersistenceMode::Transient)
                continue;

            emitter << YAML::BeginMap;
            emitter << YAML::Key << "id" << YAML::Value << id.toString();
            emitter << YAML::Key << "type" << YAML::Value << (record->type == AssetType::Model ? "model" : "material");
            if (!record->name.empty())
                emitter << YAML::Key << "name" << YAML::Value << record->name;
            if (!record->sourceUri.empty())
                emitter << YAML::Key << "uri" << YAML::Value << record->sourceUri;
            if (!record->primitiveName.empty())
                emitter << YAML::Key << "primitive" << YAML::Value << record->primitiveName;

            if (record->type == AssetType::Material)
            {
                if (const auto handle = materials.findByAssetId(id))
                {
                    if (const Material *material = materials.getMaterial(*handle))
                    {
                        writeMaterialData(emitter, material->getMaterialData());
                        writeMaterialPipeline(emitter, material->getPipelineConfig());
                    }
                }
            }
            emitter << YAML::EndMap;
        }
        emitter << YAML::EndSeq;

        // --- entities ---
        Ecs::World &world = scene.getWorld();
        Ecs::Serializer serializer(emitter, &models, &materials);

        emitter << YAML::Key << "entities" << YAML::Value << YAML::BeginSeq;
        for (const Ecs::Entity entity : scene.getEntities())
        {
            const auto guid = world.guidOf(entity);
            if (!guid)
                continue;

            emitter << YAML::BeginMap;
            emitter << YAML::Key << "id" << YAML::Value << guid->toString();
            const std::string_view name = scene.getEntityName(entity);
            if (!name.empty())
                emitter << YAML::Key << "name" << YAML::Value << std::string(name);

            emitter << YAML::Key << "components" << YAML::Value << YAML::BeginSeq;
            for (const Ecs::ComponentTypeInfo &info : world.types().all())
            {
                if (info.name == nullptr || (info.serialize == nullptr && info.descriptor == nullptr))
                    continue;   // unregistered gap or not serializable
                void *raw = info.tryGetRaw(world, entity);
                if (raw == nullptr)
                    continue;   // entity lacks this component

                emitter << YAML::BeginMap;
                emitter << YAML::Key << "type" << YAML::Value << info.name;
                // A hand-written thunk still wins where one is registered; types
                // registered with a descriptor take the generic walk.
                if (info.serialize != nullptr)
                    info.serialize(raw, serializer);
                else
                    Ecs::serializeReflected(*info.descriptor, raw, serializer);
                emitter << YAML::EndMap;
            }
            emitter << YAML::EndSeq;   // components

            emitter << YAML::EndMap;   // entity
        }
        emitter << YAML::EndSeq;       // entities

        emitter << YAML::EndMap;       // root
        return emitter.c_str();
    }
}
