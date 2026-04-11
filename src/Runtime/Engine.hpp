#pragma once

#include <array>
#include <stdio.h>
#include <stdlib.h>
#include <exception>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <memory>

#include "Assets/ModelRegistry.hpp"
#include "Core/HotReload/HotReloadManager.hpp"
#include "Core/Logging/Logger.hpp"
#include "Core/Time/FrameTimer.hpp"
#include "Editor/ImGui/EditorPanels.hpp"
#include "Platform/Input/Input.hpp"
#include "Platform/Window/Window.hpp"
#include "Renderer/Scene/RenderExtractionManager.hpp"
#include "Renderer/Resources/Model.hpp"
#include "Renderer/View/RenderView.hpp"
#include "Renderer/Vulkan/Vulkan.hpp"
#include "Renderer/Vulkan/vk_shader_manager.hpp"
#include "Scene/SceneManager.hpp"
#include "Scene/SceneQueries.hpp"
#include "Renderer/Material/TextureLoader.hpp"
#include "quill/LogMacros.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

using namespace Faye;

const uint32_t WIDTH = 1920;
const uint32_t HEIGHT = 1080;

class Engine
{
public:
    struct ImportedModelRegistration
    {
        ModelHandle handle{};
        Model::Bounds bounds{};
    };

    Engine() = default;

    void run()
    {
        glfwWindow = std::make_unique<Window>(WIDTH, HEIGHT, "[Faye] - Vulkan Renderer");

        LOG_INFO(Logger::getInstance(), "Init Vulkan...");

        vkData = std::make_unique<Vulkan>(*glfwWindow);
        modelRegistry = std::make_unique<ModelRegistry>();
        materialRegistry = std::make_unique<MaterialRegistry>();
        // textureRegistry = std::make_unique<TextureRegistry>();
        renderExtractionManager = std::make_unique<RenderExtractionManager>();
        sceneManager = std::make_unique<SceneManager>();
        initializeScene();
        configureHotReload();
        editorPanels.setPrimitiveCreateCallback([this](PrimitiveType primitiveType)
                                                { return createPrimitiveEntity(primitiveType); });
        editorPanels.setMaterialRegistry(materialRegistry.get());
        editorPanels.setModelRegistry(modelRegistry.get());
        editorPanels.setTextureThumbnailCallback([this](MaterialHandle handle, TextureType textureType) -> ImTextureID
                                                {
                                                    if (vkData == nullptr || materialRegistry == nullptr)
                                                    {
                                                        return 0;
                                                    }

                                                    const Material *material = materialRegistry->getMaterial(handle);
                                                    if (material == nullptr)
                                                    {
                                                        return 0;
                                                    }

                                                    return reinterpret_cast<ImTextureID>(
                                                        vkData->getMaterialTextureThumbnail(handle, *material, textureType));
                                                });

        hotReloadShaderSubscriptionToken = hotReloadManager.subscribe([this](const HotReloadEvent &event)
                                                                      { HotReloadShaderCompilation(event); }, std::vector<std::string_view>{"shader-sources"});
        hotReloadShaderSubscriptionTokenTwo = hotReloadManager.subscribe([this](const HotReloadEvent &event)
                                                                         { LOG_INFO(Logger::getInstance(), "Received hot reload event for watchId '{}' and path '{}'", event.watchId, event.path.string()); });
        hotReloadManager.start();

        LOG_INFO(Logger::getInstance(), "Starting main loop...");
        mainLoop();

        if (hotReloadShaderSubscriptionToken != 0)
        {
            hotReloadManager.unsubscribe(hotReloadShaderSubscriptionToken);
            hotReloadShaderSubscriptionToken = 0;
        }
        if (hotReloadShaderSubscriptionTokenTwo != 0)
        {
            hotReloadManager.unsubscribe(hotReloadShaderSubscriptionTokenTwo);
            hotReloadShaderSubscriptionTokenTwo = 0;
        }

        hotReloadManager.stop();
    }

private:
    static constexpr size_t primitiveIndex(PrimitiveType primitiveType)
    {
        return static_cast<size_t>(primitiveType);
    }

    // Custom class for glfw window related functionality.
    std::unique_ptr<Window> glfwWindow;

    // Custom class for Vulkan init/functionality.
    std::unique_ptr<Vulkan> vkData;
    EditorPanels editorPanels;

    std::unique_ptr<ModelRegistry> modelRegistry;
    std::unique_ptr<MaterialRegistry> materialRegistry;
    // std::unique_ptr<TextureRegistry> textureRegistry;
    std::unique_ptr<RenderExtractionManager> renderExtractionManager;
    std::unique_ptr<SceneManager> sceneManager;
    HotReloadManager hotReloadManager;
    HotReloadManager::CallbackToken hotReloadShaderSubscriptionToken = 0;
    HotReloadManager::CallbackToken hotReloadShaderSubscriptionTokenTwo = 0;
    VulkanShaderManager shaderManager;
    FrameTimer timer;
    std::array<ModelHandle, static_cast<size_t>(PrimitiveType::Count)> primitiveModelHandles{};
    Entity activeCameraEntity;
    Entity postProcessSettingsEntity;
    bool editorViewportHovered = true;
    bool editorViewportFocused = true;
    RenderDebugMode editorViewportDebugMode = RenderDebugMode::Lit;

    void configureHotReload()
    {
        hotReloadManager.clearWatches();
        hotReloadManager.addWatch({
            .id = "post-process-effects",
            .rootPath = "./src/Assets/PostProcessEffects",
            .fileExtensions = {".ppfx"},
            .recursive = true,
        });
        hotReloadManager.addWatch({
            .id = "shader-sources",
            .rootPath = "./src/shaders",
            .fileExtensions = {".vert", ".frag", ".comp"},
            .recursive = true,
        });
    }

    void HotReloadShaderCompilation(const HotReloadEvent &event)
    {
        LOG_INFO(Logger::getInstance(), "Hot reload change detected: {}", event.path.filename().string());

        if (event.watchId != "shader-sources" || event.type != HotReloadEventType::Modified)
        {
            LOG_INFO(Logger::getInstance(), "Ignoring hot reload event for watchId '{}' and path '{}'", event.watchId, event.path.string());
            return;
        }

        const std::filesystem::path &path = event.path;
        if (path.extension() == ".vert" || path.extension() == ".frag" || path.extension() == ".comp")
        {
            LOG_INFO(Logger::getInstance(), "Recompiling shader: {}", path.filename().string());
            std::string compileResult = shaderManager.shaderFileChange(path);
            if (!compileResult.empty())
            {
                vkData->notifyShaderRecompilation(compileResult);
            }
        }
    }

    ModelHandle ensurePrimitiveHandle(PrimitiveType primitiveType)
    {
        ModelHandle &handle = primitiveModelHandles[primitiveIndex(primitiveType)];
        if (!handle.isValid())
        {
            handle = modelRegistry->registerModel(Model::createPrimitive(*vkData->getVkDevice(), primitiveType));
        }

        return handle;
    }

    ModelHandle registerImportedModel(const std::string &modelPath, MaterialPipelineConfig pipelineConfig = {})
    {
        return registerImportedModelWithBounds(modelPath, std::move(pipelineConfig)).handle;
    }

    ImportedModelRegistration registerImportedModelWithBounds(const std::string &modelPath, MaterialPipelineConfig pipelineConfig = {})
    {
        std::unique_ptr<Model> model = Model::createModelFromFile(*vkData->getVkDevice(), modelPath);
        const Model::Bounds bounds = model->getLocalBounds();

        const auto &importedMaterials = model->getImportedMaterials();
        if (!importedMaterials.empty())
        {
            std::vector<MaterialHandle> importedMaterialHandles;
            importedMaterialHandles.reserve(importedMaterials.size());

            for (const MaterialData &materialData : importedMaterials)
            {
                importedMaterialHandles.push_back(materialRegistry->registerMaterial(materialData, pipelineConfig));
            }

            model->assignImportedMaterialHandles(importedMaterialHandles);
        }

        return ImportedModelRegistration{
            modelRegistry->registerModel(std::move(model)),
            bounds};
    }

    Entity createPrimitiveEntity(PrimitiveType primitiveType)
    {
        if (sceneManager == nullptr || !sceneManager->hasActiveScene())
        {
            return {};
        }

        Scene &scene = sceneManager->getActiveScene();
        Entity entity = scene.createEntity(std::string(primitiveTypeName(primitiveType)));
        entity.addTransform();

        auto &mesh = entity.addMesh(ensurePrimitiveHandle(primitiveType));
        mesh.materialHandle = materialRegistry->registerMaterial(MaterialData{"Default Material", glm::vec3(1.0f, 1.0f, 1.0f)});

        return entity;
    }

    void initializeScene()
    {
        Scene &scene = sceneManager->createScene("Main Scene");

        postProcessSettingsEntity = scene.createEntity("Post Processing");
        postProcessSettingsEntity.addPostProcessStack() = makeDefaultPostProcessStack();

        Entity editorCamera = scene.createEntity("Editor Camera");
        activeCameraEntity = editorCamera;
        editorCamera.addTransform();
        editorCamera.addCamera(true);

        MaterialHandle defaultMat = materialRegistry->registerMaterial(MaterialData{"Default Material", glm::vec3(1.0f, 1.0f, 1.0f)});
        MaterialHandle redMat = materialRegistry->registerMaterial(MaterialData{"Red Material", glm::vec3(1.0f, 0.0f, 0.0f)});
        MaterialHandle greenMat = materialRegistry->registerMaterial(MaterialData{"Green Material", glm::vec3(0.0f, 1.0f, 0.0f)});
        MaterialHandle blueMat = materialRegistry->registerMaterial(MaterialData{"Blue Material", glm::vec3(0.0f, 0.0f, 1.0f)});
        MaterialHandle shinyMat = materialRegistry->registerMaterial(MaterialData{"Shiny Material", glm::vec3(1.0f, 1.0f, 1.0f), {}, glm::vec4(1.0f), glm::vec4(0.0f, 1.0f, 1.0f, 1.0f), glm::vec4(1.0f, 1.0f, 1.0f, 32.0f)});

        MaterialData spyBoxMaterial{"Spy Box Material", glm::vec3(1.0f, 1.0f, 1.0f)};
        spyBoxMaterial.textures.push_back(loadTextureFromFile("src/textures/spy.jpg", TextureType::Albedo));
        spyBoxMaterial.baseColorFactor = glm::vec4(1.0f);
        spyBoxMaterial.metallicFactor = 0.0f;
        spyBoxMaterial.roughnessFactor = 0.85f;

        MaterialHandle spyBoxMat = materialRegistry->registerMaterial(std::move(spyBoxMaterial));

        Entity meshEntity = scene.createEntity("Cube A");
        auto &meshTransform = meshEntity.addTransform();
        auto meshHandle = ensurePrimitiveHandle(PrimitiveType::Cube);
        auto &meshComponent = meshEntity.addMesh(meshHandle);
        meshTransform.translation = {-1.f, 0.f, -1.f};
        meshTransform.rotation = glm::vec3(45.f, 0.f, 0.f);
        meshTransform.scale = {.5f, .5f, .5f};
        meshComponent.materialHandle = redMat;

        Entity secondMeshEntity = scene.createEntity("Cube B");
        auto &secondMeshTransform = secondMeshEntity.addTransform();
        auto secondMeshHandle = ensurePrimitiveHandle(PrimitiveType::Cube);
        auto &secondMeshComponent = secondMeshEntity.addMesh(secondMeshHandle);
        secondMeshTransform.translation = {2.f, 0.f, -1.f};
        secondMeshTransform.rotation = glm::vec3(45.f, 0.f, 0.f);
        secondMeshTransform.scale = {.5f, .5f, .5f};
        secondMeshComponent.materialHandle = greenMat;

        // Entity thirdMeshEntity = scene.createEntity("Cube C");
        // auto &thirdMeshTransform = thirdMeshEntity.addTransform();
        // auto thirdMeshHandle = ensurePrimitiveHandle(PrimitiveType::Cube);
        // auto &thirdMeshComponent = thirdMeshEntity.addMesh(thirdMeshHandle);
        // thirdMeshTransform.translation = {2.f, 0.f, 1.f};
        // thirdMeshTransform.rotation = glm::vec3(45.f, 0.f, 0.f);
        // thirdMeshTransform.scale = {.5f, .5f, .5f};
        // thirdMeshComponent.materialHandle = spyBoxMat;

        Entity floorEntity = scene.createEntity("Floor");
        auto &floorTransform = floorEntity.addTransform();
        auto floorHandle = ensurePrimitiveHandle(PrimitiveType::Plane);
        auto &floorMeshComponent = floorEntity.addMesh(floorHandle);
        floorTransform.translation = {0.f, -0.5f, 0.f};
        floorTransform.scale = {3.f, 1.f, 3.f};
        floorMeshComponent.materialHandle = defaultMat;

        Entity pointLightEntity = scene.createEntity("Point Light");
        auto &pointLightTransform = pointLightEntity.addTransform();
        auto &pointLightComponent = pointLightEntity.addPointLight();
        pointLightTransform.translation = {0.f, -1.0f, 1.25f};
        pointLightComponent.color = {0.f, 0.f, 1.f};
        pointLightComponent.intensity = 1.5f;
        pointLightComponent.radius = 0.1f;

        Entity pointLightEntity2 = scene.createEntity("Point Light Green");
        auto &pointLightTransform2 = pointLightEntity2.addTransform();
        auto &pointLightComponent2 = pointLightEntity2.addPointLight();
        pointLightTransform2.translation = {0.f, 1.0f, 1.25f};
        pointLightComponent2.color = {0.f, 1.f, 0.f};
        pointLightComponent2.intensity = 1.5f;
        pointLightComponent2.radius = 0.1f;

        Entity pointLightEntity3 = scene.createEntity("Point Light Red");
        auto &pointLightTransform3 = pointLightEntity3.addTransform();
        auto &pointLightComponent3 = pointLightEntity3.addPointLight();
        pointLightTransform3.translation = {0.f, 1.0f, -1.25f};
        pointLightComponent3.color = {1.0f, 0.0f, 0.0f};
        pointLightComponent3.intensity = 1.5f;
        pointLightComponent3.radius = 0.1f;

        // Entity model = scene.createEntity("Viking Room");
        // auto &modelTransform = model.addTransform();
        // auto &modelMesh = model.addMesh(registerImportedModel("src/include/viking_room.obj"));
        // modelTransform.translation = {0.f, -0.5f, 0.f};
        // modelTransform.rotation = {0.f, 180.f, 0.f};
        // modelTransform.scale = {1.0f, 1.0f, 1.0f};
        // modelMesh.materialHandle = defaultMat;

        Entity modelgltf = scene.createEntity("Test gltf");
        auto &modelgltfTransform = modelgltf.addTransform();
        auto &modelgltfMesh = modelgltf.addMesh(registerImportedModel("src/include/assimp/test/models/glTF/BoxTextured-glTF/BoxTextured.gltf"));
        modelgltfTransform.translation = {0.f, -0.5f, 2.f};
        modelgltfTransform.rotation = {0.f, 180.f, 0.f};
        modelgltfTransform.scale = {1.0f, 1.0f, 1.0f};
        modelgltfMesh.materialHandle = defaultMat;

        Entity modelAdam = scene.createEntity("Adam Model");
        auto &modelAdamTransform = modelAdam.addTransform();
        const ImportedModelRegistration adamRegistration = registerImportedModelWithBounds("src/include/models/adamHead/adamHead.gltf");
        modelAdam.addMesh(adamRegistration.handle);
        modelAdamTransform.translation = {0.f, 0.f, 0.f};
        modelAdamTransform.rotation = {0.f, 0.f, 0.f};
        modelAdamTransform.scale = {1.0f, 1.0f, 1.0f};
    }

    void mainLoop()
    {
        Input &input = Input::getInstance();
        Scene &scene = sceneManager->getActiveScene();
        editorPanels.bindScene(&scene);

        timer.frameStart();

        while (!glfwWindow->shouldClose())
        {
            glfwPollEvents();
            hotReloadManager.dispatchPendingEvents();

            timer.frameEnd();

            auto *cameraTransform = activeCameraEntity.tryGetTransform();
            auto *cameraComponent = activeCameraEntity.tryGetCamera();
            if (cameraTransform == nullptr || cameraComponent == nullptr)
            {
                throw std::runtime_error("Active scene camera is not configured correctly");
            }

            // Use the actual scene render resolution (driven by the viewport panel)
            // so the camera projection matches what is rendered. Falls back to the
            // window extent on the first frame before any panel-driven resize.
            VkExtent2D sceneExtent = vkData->getSceneRenderExtent();
            if (sceneExtent.width == 0 || sceneExtent.height == 0)
                sceneExtent = glfwWindow->getExtent();

            RenderView renderView{
                &cameraComponent->camera,
                {sceneExtent.width, sceneExtent.height},
                RenderOutputTarget::OffscreenSceneColor,
                editorViewportDebugMode};

            cameraComponent->camera.saveViewProjectionMatrix();
            input.updateEditorCamera(
                glfwWindow->getWindow(),
                *cameraTransform,
                static_cast<float>(timer.getDelta()),
                {editorViewportHovered, editorViewportFocused});
            cameraComponent->camera.setViewYXZ(cameraTransform->translation, cameraTransform->rotation);
            cameraComponent->camera.setPerspectiveProjection(glm::radians(50.f), renderView.viewport.aspectRatio(), 0.1f, 100.f);

            if (input.isKeyPressed(glfwWindow->getWindow(), input.keyMap.escape))
            {
                glfwSetWindowShouldClose(glfwWindow->getWindow(), true);
            }

            RenderSceneSnapshot renderScene = renderExtractionManager->extract(scene, *modelRegistry, *materialRegistry);

            VulkanFrameInput frameInput{
                renderView,
                renderScene,
                postProcessSettingsEntity.tryGetPostProcessStack(),
                static_cast<int>(timer.getFrameTime(1)),
                static_cast<int>(timer.getAverageFPS())};

            vkData->renderFrame(frameInput, [this, &scene, cameraComponent](ImGuiFrameData &frameData)
                                {
                                    editorPanels.draw(frameData);

                                    if (frameData.viewportClicked)
                                    {
                                        const auto hit = raycastScene(
                                            scene,
                                            *modelRegistry,
                                            cameraComponent->camera,
                                            {frameData.viewportClickUv.x, frameData.viewportClickUv.y});
                                        editorPanels.setSelectedEntity(hit.has_value() ? scene.getEntity(hit->entity) : Entity{});
                                    }

                                    editorViewportHovered = frameData.viewportHovered;
                                    editorViewportFocused = frameData.viewportFocused;
                                    editorViewportDebugMode = frameData.viewportDebugMode; });
        }
    }
};