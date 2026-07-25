#include "Scene/SceneBuilder.hpp"

#include <filesystem>
#include <vector>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
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
                                 ScriptSystem &scripts,
                                 LuaScriptSystem &luaScripts)
    : models(models), materials(materials), scripts(scripts), luaScripts(luaScripts) {}

Faye::ModelHandle Faye::SceneBuilder::ensurePrimitiveHandle(PrimitiveType primitiveType)
{
    ModelHandle &handle = primitiveModelHandles[primitiveIndex(primitiveType)];
    if (!handle.isValid())
    {
        handle = models.createPrimitive(primitiveType);
    }

    return handle;
}

Faye::SceneBuilder::ImportedModelRegistration Faye::SceneBuilder::registerImportedModelWithBounds(
    const std::string &modelPath, MaterialPipelineConfig pipelineConfig)
{
    std::unique_ptr<Model> model = models.makeModelFromFile(modelPath);
    const Model::Bounds bounds = model->getLocalBounds();

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

    return ImportedModelRegistration{
        models.registerModel(std::move(model)),
        bounds};
}

Faye::Entity Faye::SceneBuilder::createPrimitiveEntity(Scene &scene, PrimitiveType primitiveType)
{
    Entity entity = scene.createEntity(std::string(primitiveTypeName(primitiveType)));
    auto &transform = entity.add<TransformComponent>();

    auto &mesh = entity.addMesh(ensurePrimitiveHandle(primitiveType));

    if (primitiveType == PrimitiveType::WaterPlane)
    {
        mesh.materialHandle = waterMaterialHandle;

        // Scale up to a large world-space surface (20×20 metres by default).
        transform.scale = {20.0f, 1.0f, 20.0f};

        // Add WaterComponent so the subdivision count is inspectable.
        auto &water = entity.add<WaterComponent>();
        water.subdivisions = 64;

        // Attach the subdivision script — it watches WaterComponent.subdivisions
        // and rebuilds the mesh when the value changes.
        scripts.attachBuiltinScript(entity,
                                    new WaterSubdivisionScript([this](uint32_t divs) -> ModelHandle
                                                               { return models.createPrimitive(PrimitiveType::WaterPlane, divs); }),
                                    "WaterSubdivision");
    }
    else
    {
        mesh.materialHandle = materials.registerMaterial(MaterialData{"Default Material", glm::vec3(1.0f, 1.0f, 1.0f)});
    }

    return entity;
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

    MaterialHandle defaultMat = materials.registerMaterial(MaterialData{"Default Material", glm::vec3(1.0f, 1.0f, 1.0f)});
    MaterialHandle redMat = materials.registerMaterial(MaterialData{"Red Material", glm::vec3(1.0f, 0.0f, 0.0f)});
    MaterialHandle greenMat = materials.registerMaterial(MaterialData{"Green Material", glm::vec3(0.0f, 1.0f, 0.0f)});
    MaterialHandle blueMat = materials.registerMaterial(MaterialData{"Blue Material", glm::vec3(0.0f, 0.0f, 1.0f)});
    MaterialHandle shinyMat = materials.registerMaterial(MaterialData{"Shiny Material", glm::vec3(1.0f, 1.0f, 1.0f), {}, glm::vec4(1.0f), glm::vec4(0.0f, 1.0f, 1.0f, 1.0f), glm::vec4(1.0f, 1.0f, 1.0f, 32.0f)});
    (void)blueMat;
    (void)shinyMat;

    // Water material — uses water.vert/water.frag via MaterialPipelineConfig.
    // The pipeline is selected per-material by SimpleRenderSystem at draw time.
    // Texture slot mapping (fixed by MaterialCache::writeDescriptorSet):
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
    waterPipelineConfig.enableAlphaBlending = true;   // water alpha is meaningful
    waterPipelineConfig.domain = MaterialDomain::Water; // excluded from depth prepass
    waterMaterialHandle = materials.registerMaterial(
        std::move(waterMaterialData),
        std::move(waterPipelineConfig));

    MaterialData spyBoxMaterial{"Spy Box Material", glm::vec3(1.0f, 1.0f, 1.0f)};
    spyBoxMaterial.textures.push_back(loadTextureFromFile(Paths::resolve("src/textures/spy.jpg").string(), TextureType::Albedo));
    spyBoxMaterial.baseColorFactor = glm::vec4(1.0f);
    spyBoxMaterial.metallicFactor = 0.0f;
    spyBoxMaterial.roughnessFactor = 0.85f;

    MaterialHandle spyBoxMat = materials.registerMaterial(std::move(spyBoxMaterial));
    (void)spyBoxMat;

    Entity meshEntity = scene.createEntity("Cube A");
    auto &meshTransform = meshEntity.add<TransformComponent>();
    auto meshHandle = ensurePrimitiveHandle(PrimitiveType::Cube);
    auto &meshComponent = meshEntity.addMesh(meshHandle);
    meshTransform.translation = {-1.f, 0.f, -1.f};
    meshTransform.rotation = glm::vec3(45.f, 0.f, 0.f);
    meshTransform.scale = {.5f, .5f, .5f};
    meshComponent.materialHandle = redMat;

    Entity secondMeshEntity = scene.createEntity("Cube B");
    auto &secondMeshTransform = secondMeshEntity.add<TransformComponent>();
    auto secondMeshHandle = ensurePrimitiveHandle(PrimitiveType::Cube);
    auto &secondMeshComponent = secondMeshEntity.addMesh(secondMeshHandle);
    secondMeshTransform.translation = {2.f, 0.f, -1.f};
    secondMeshTransform.rotation = glm::vec3(45.f, 0.f, 0.f);
    secondMeshTransform.scale = {.5f, .5f, .5f};
    secondMeshComponent.materialHandle = greenMat;

    Entity floorEntity = scene.createEntity("Floor");
    auto &floorTransform = floorEntity.add<TransformComponent>();
    auto floorHandle = ensurePrimitiveHandle(PrimitiveType::Plane);
    auto &floorMeshComponent = floorEntity.addMesh(floorHandle);
    floorTransform.translation = {0.f, -0.5f, 0.f};
    floorTransform.scale = {3.f, 1.f, 3.f};
    floorMeshComponent.materialHandle = defaultMat;

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
    sunLight.intensity = 1.0f;

    Entity modelAdam = scene.createEntity("Adam Model");
    auto &modelAdamTransform = modelAdam.add<TransformComponent>();
    const ImportedModelRegistration adamRegistration = registerImportedModelWithBounds("src/include/models/adamHead/adamHead.gltf");
    modelAdam.addMesh(adamRegistration.handle);
    modelAdamTransform.translation = {0.f, 0.f, 0.f};
    modelAdamTransform.rotation = {0.f, 0.f, 0.f};
    modelAdamTransform.scale = {1.0f, 1.0f, 1.0f};

    // Demo scripts: the scripting systems have already bound this scene in
    // OnPostInit, so onStart fires correctly here.
    //
    // Native .so is absent on a fresh build until faye_rotator_script is compiled.
    scripts.loadScript(meshEntity, "bin/libfaye_rotator_script.so");

    if (std::filesystem::exists("src/Scripting/ExampleScripts/rotator.lua"))
    {
        luaScripts.loadScript(secondMeshEntity, "src/Scripting/ExampleScripts/rotator.lua", &scene);
    }

    LOG_INFO(Logger::get(), "Scene populated in {} ms", initSceneTimer.elapsedMs());
    return setup;
}
