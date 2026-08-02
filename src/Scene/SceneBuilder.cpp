#include "Scene/SceneBuilder.hpp"

#include <filesystem>
#include <vector>

#include <glm/glm.hpp>

#include "Core/Logging/Logger.hpp"
#include "Core/Path/Paths.hpp"
#include "Core/Time/Timer.hpp"
#include "Renderer/Material/TextureLoader.hpp"
#include "Renderer/PostProcess/PostProcessEffectLibrary.hpp"
#include "Scripting/ScriptSystem.hpp"
#include "Scripting/LuaScriptSystem.hpp"
#include "Scripting/BuiltinScripts/WaterSubdivisionScript.hpp"
#include "quill/LogMacros.h"

using namespace Faye;

Faye::SceneBuilder::SceneBuilder(ModelRegistry &models,
                                 MaterialRegistry &materials,
                                 AssetDatabase &assetDatabase,
                                 ScriptSystem &scripts,
                                 LuaScriptSystem &luaScripts,
                                 Jobs::JobSystem &jobSystem)
    : models(models), materials(materials), assetDatabase(assetDatabase),
      scripts(scripts), luaScripts(luaScripts), jobSystem(jobSystem) {}

Faye::ModelHandle Faye::SceneBuilder::ensurePrimitiveHandle(PrimitiveType primitiveType)
{
    ModelHandle &handle = primitiveModelHandles[primitiveIndex(primitiveType)];
    if (!handle.isValid())
    {
        handle = models.createPrimitive(primitiveType);
    }

    return handle;
}

Faye::MaterialHandle Faye::SceneBuilder::ensureWaterMaterial()
{
    if (waterMaterialHandle.isValid())
    {
        return waterMaterialHandle;
    }

    // A scene loaded from disk registers its water material under the
    // deterministic built-in id; reuse it so editor-added water planes share
    // the loaded material instead of falling back to white.
    if (const auto builtIn = materials.findByAssetId(AssetDatabase::idForBuiltIn("Water Material")))
    {
        waterMaterialHandle = *builtIn;
    }
    if (waterMaterialHandle.isValid())
    {
        return waterMaterialHandle;
    }

    // Default water material — uses water.vert/water.frag via
    // MaterialPipelineConfig. The pipeline is selected per-material by
    // SimpleRenderSystem at draw time. Texture slot mapping (fixed by
    // MaterialCache::writeDescriptorSet):
    //   Albedo   (binding 0) → normalMap1 in water.frag → waternormal1.jpg
    //   Normal   (binding 1) → normalMap2 in water.frag → waternormal2.jpg
    //   Metallic (binding 2) → foamMap    in water.frag → waterfoam1.jpg
    MaterialData waterMaterialData{"Water", glm::vec3(0.05f, 0.50f, 0.55f)};
    waterMaterialData.shininess = 64.0f;  // specularShininess.w  -- editor: Shininess
    waterMaterialData.normalScale = 0.4f; // surfaceFactors.z     -- editor: Normal Scale
    waterMaterialData.textures.push_back(loadTextureFromFile(Paths::resolve("src/textures/waternormal1.jpg").string(), TextureType::Albedo));
    waterMaterialData.textures.push_back(loadTextureFromFile(Paths::resolve("src/textures/waternormal2.jpg").string(), TextureType::Normal));
    waterMaterialData.textures.push_back(loadTextureFromFile(Paths::resolve("src/textures/waterfoam1.jpg").string(), TextureType::Metallic));
    MaterialPipelineConfig waterPipelineConfig{"water.vert", "water.frag"};
    waterPipelineConfig.enableAlphaBlending = true;    // water alpha is meaningful
    waterPipelineConfig.domain = MaterialDomain::Water; // excluded from depth prepass
    const AssetId waterAssetId = AssetDatabase::idForBuiltIn("Water Material");
    waterMaterialHandle = materials.registerMaterial(
        std::move(waterMaterialData),
        std::move(waterPipelineConfig),
        waterAssetId);
    assetDatabase.registerAsset(AssetRecord{
        .id = waterAssetId,
        .type = AssetType::Material,
        .name = "Water Material",
        .persistence = AssetPersistenceMode::BuiltIn,
    });
    return waterMaterialHandle;
}

Faye::SceneBuilder::ImportedModelRegistration Faye::SceneBuilder::registerImportedModelWithBounds(
    const std::string &modelPath, MaterialPipelineConfig pipelineConfig)
{
    // Dedup by deterministic asset id: a second import of the same uri reuses
    // the already-imported model (and skips re-registering its materials).
    const AssetId assetId = AssetDatabase::idForSourceUri(modelPath);
    const bool alreadyImported = models.findByAssetId(assetId).has_value();

    const ModelHandle handle = models.getOrImportByUri(modelPath);
    Model *model = models.getModel(handle);
    const Model::Bounds bounds = model->getLocalBounds();

    // Imported (embedded) materials are registered once, on the first import,
    // and assigned to the model's submeshes.
    if (!alreadyImported)
    {
        const auto &importedMaterials = model->getImportedMaterials();
        if (!importedMaterials.empty())
        {
            std::vector<MaterialHandle> importedMaterialHandles;
            importedMaterialHandles.reserve(importedMaterials.size());

            for (const MaterialData &materialData : importedMaterials)
            {
                importedMaterialHandles.push_back(materials.registerMaterial(materialData, pipelineConfig));
            }

            model->assignImportedMaterialHandles(importedMaterialHandles);
        }
    }

    return ImportedModelRegistration{
        handle,
        bounds};
}

Faye::Entity Faye::SceneBuilder::createPrimitiveEntity(Scene &scene, PrimitiveType primitiveType)
{
    Entity entity = scene.createEntity(std::string(primitiveTypeName(primitiveType)));
    auto &transform = entity.add<TransformComponent>();

    auto &mesh = entity.addMesh(ensurePrimitiveHandle(primitiveType));

    if (primitiveType == PrimitiveType::WaterPlane)
    {
        mesh.materialHandle = ensureWaterMaterial();

        // Scale up to a large world-space surface (20×20 metres by default).
        transform.scale = {20.0f, 1.0f, 20.0f};

        // Add WaterComponent so the subdivision count is inspectable.
        auto &water = entity.add<WaterComponent>();
        water.subdivisions = 64;

        // Attach the subdivision script — it watches WaterComponent.subdivisions
        // and rebuilds the mesh when the value changes.
        scripts.attachBuiltinScript(entity,
                                    new WaterSubdivisionScript([this](uint32_t divs) -> ModelHandle
                                                               { return models.recreatePrimitive(PrimitiveType::WaterPlane, divs); }),
                                    "WaterSubdivision");
    }
    else
    {
        mesh.materialHandle = materials.registerMaterial(MaterialData{"Default Material", glm::vec3(1.0f, 1.0f, 1.0f)});
    }

    return entity;
}

/**
 * TODO: Create switch which handles different file types.
 * so if 3d models are passed in, the models are imported.
 * if mats are imported, they are loaded and created, etc.
 */

Entity Faye::SceneBuilder::importAndCreateModel(Scene &scene, std::filesystem::path path)
{
    // Implementation for importing and creating a model entity
    try 
    {
        const ImportedModelRegistration reg = registerImportedModelWithBounds(path);
        Entity entity = scene.createEntity(path.filename().string());
        auto &t = entity.add<TransformComponent>();
        entity.addMesh(reg.handle);
        t.translation = {0.f, 0.f, 0.f};
        t.rotation = {0.f, 0.f, 0.f};
        t.scale = {1.0f, 1.01f, 1.0f};
        return entity;
    }
    catch (const std::exception &e)
    {
        LOG_ERROR(Logger::get(), "Failed to import model at path {}. Error: {}", path.string(), e.what());
        throw;
    }
}

void Faye::SceneBuilder::populateDefaultScene(Scene &scene)
{
    Time::StopWatch initSceneTimer;

    std::filesystem::path defaultAssetPath = Paths::projects();

    MaterialHandle defaultMat = materials.registerMaterial(MaterialData{"Default Material", glm::vec3(1.0f, 1.0f, 1.0f)});
    MaterialHandle redMat = materials.registerMaterial(MaterialData{"Red Material", glm::vec3(1.0f, 0.0f, 0.0f)});
    MaterialHandle greenMat = materials.registerMaterial(MaterialData{"Green Material", glm::vec3(0.0f, 1.0f, 0.0f)});
    MaterialHandle blueMat = materials.registerMaterial(MaterialData{"Blue Material", glm::vec3(0.0f, 0.0f, 1.0f)});
    MaterialHandle shinyMat = materials.registerMaterial(MaterialData{"Shiny Material", glm::vec3(1.0f, 1.0f, 1.0f), {}, glm::vec4(1.0f), glm::vec4(0.0f, 1.0f, 1.0f, 1.0f), glm::vec4(1.0f, 1.0f, 1.0f, 32.0f)});
    (void)blueMat;
    (void)shinyMat;

    Entity meshEntity = createPrimitiveEntity(scene, PrimitiveType::Cube);
    meshEntity.tryGet<TransformComponent>()->translation = {-1.f, 0.f, -1.f};
    meshEntity.tryGet<TransformComponent>()->rotation = glm::vec3(45.f, 0.f, 0.f);
    meshEntity.tryGet<TransformComponent>()->scale = {.5f, .5f, .5f};
    meshEntity.tryGet<MeshRendererComponent>()->materialHandle = redMat;

    Entity secondMeshEntity = createPrimitiveEntity(scene, PrimitiveType::Cube);
    secondMeshEntity.tryGet<TransformComponent>()->translation = {2.f, 0.f, -1.f};
    secondMeshEntity.tryGet<TransformComponent>()->rotation = glm::vec3(45.f, 0.f, 0.f);
    secondMeshEntity.tryGet<TransformComponent>()->scale = {.5f, .5f, .5f};
    secondMeshEntity.tryGet<MeshRendererComponent>()->materialHandle = greenMat;

    Entity floorEntity = createPrimitiveEntity(scene, PrimitiveType::Plane);
    floorEntity.tryGet<TransformComponent>()->translation = {0.f, -0.5f, 0.f};
    floorEntity.tryGet<TransformComponent>()->scale = {5.f, 1.f, 5.f};
    floorEntity.tryGet<MeshRendererComponent>()->materialHandle = defaultMat;

    Entity pointLightEntity = scene.createEntity("Point Light");
    auto &pointLightTransform = pointLightEntity.add<TransformComponent>();
    auto &pointLightComponent = pointLightEntity.add<PointLightComponent>();
    pointLightTransform.translation = {0.f, -1.0f, 1.25f};
    pointLightComponent.color = {0.f, 0.f, 1.f};
    pointLightComponent.intensity = 1.5f;
    pointLightComponent.radius = 0.1f;

    Entity pointLightEntity2 = scene.createEntity("Point Light Green");
    auto &pointLightTransform2 = pointLightEntity2.add<TransformComponent>();
    auto &pointLightComponent2 = pointLightEntity2.add<PointLightComponent>();
    pointLightTransform2.translation = {0.f, 1.0f, 1.25f};
    pointLightComponent2.color = {0.f, 1.f, 0.f};
    pointLightComponent2.intensity = 1.5f;
    pointLightComponent2.radius = 0.1f;

    Entity pointLightEntity3 = scene.createEntity("Point Light Red");
    auto &pointLightTransform3 = pointLightEntity3.add<TransformComponent>();
    auto &pointLightComponent3 = pointLightEntity3.add<PointLightComponent>();
    pointLightTransform3.translation = {0.f, 1.0f, -1.25f};
    pointLightComponent3.color = {1.0f, 0.0f, 0.0f};
    pointLightComponent3.intensity = 1.5f;
    pointLightComponent3.radius = 0.1f;

    // Directional "sun": direction comes from the Transform rotation (pitched
    // down and angled), so rotating this entity aims the light.
    Entity sunEntity = scene.createEntity("Directional Light");
    auto &sunTransform = sunEntity.add<TransformComponent>();
    auto &sunLight = sunEntity.add<DirectionalLightComponent>();
    sunTransform.rotation = {0.9f, 0.4f, 0.f};
    sunLight.color = {1.0f, 0.96f, 0.9f};
    sunLight.intensity = 0.2f;

    // Entity modelAdam = scene.createEntity("Adam Model");
    // auto &modelAdamTransform = modelAdam.add<TransformComponent>();
    // const ImportedModelRegistration adamRegistration = registerImportedModelWithBounds("src/include/models/adamHead/adamHead.gltf");
    // modelAdam.addMesh(adamRegistration.handle);
    // modelAdamTransform.translation = {0.f, 0.f, 0.f};
    // modelAdamTransform.rotation = {0.f, 0.f, 0.f};
    // modelAdamTransform.scale = {1.0f, 1.0f, 1.0f};

    try {
        Entity shipModel = importAndCreateModel(scene, defaultAssetPath.string() + "/projects/models/stylized-pirate-ship/source/Ship_Scene.fbx");
        shipModel.tryGet<TransformComponent>()->scale = {0.01f, 0.01f, 0.01f};

        Entity rock = importAndCreateModel(scene, defaultAssetPath.string() + "/projects/models/stylized-stones-minipack/source/model/stones_v2.fbx");
        rock.tryGet<TransformComponent>()->scale = {0.01f, 0.01f, 0.01f};
    } catch (const std::exception &e) {
        LOG_ERROR(Logger::get(), "Failed to import a default model. Error: {}", e.what());
    }

    // Demo scripts: the scripting systems have already bound this scene in
    // OnPostInit, so onStart fires correctly here.
    //
    // Native .so is absent on a fresh build until faye_rotator_script is compiled.
    scripts.loadScript(meshEntity, "bin/libfaye_rotator_script.so");

    if (std::filesystem::exists("src/Scripting/ExampleScripts/rotator.lua"))
    {
        luaScripts.loadScript(secondMeshEntity, "src/Scripting/ExampleScripts/rotator.lua", &scene);
    }

    LOG_INFO(Logger::get(), "Scene assets loaded in {} ms", initSceneTimer.elapsedMs());
}

Faye::SceneBuilder::SceneSetup Faye::SceneBuilder::populate(Scene &scene)
{
    Time::StopWatch initSceneTimer;

    SceneSetup setup;

    setup.postProcessSettings = scene.createEntity("Post Processing");
    setup.postProcessSettings.add<PostProcessStackComponent>() = makeDefaultPostProcessStack();

    Entity editorCamera = scene.createEntity("Editor Camera");
    setup.activeCamera = editorCamera;
    editorCamera.add<TransformComponent>();
    editorCamera.addCamera(true);

    // Water material — created lazily via ensureWaterMaterial() (alpha-blended,
    // water domain, real maps). populateDefaultScene's water plane and any
    // editor-added water plane share it.
    ensureWaterMaterial();

    populateDefaultScene(scene);

    LOG_INFO(Logger::get(), "Scene populated in {} ms", initSceneTimer.elapsedMs());
    return setup;
}
