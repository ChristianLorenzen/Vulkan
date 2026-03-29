#pragma once

#include <array>
#include <stdio.h>
#include <stdlib.h>
#include <exception>
#include <iostream>
#include <fstream>
#include <memory>

#include "Assets/ModelRegistry.hpp"
#include "Core/Logging/Logger.hpp"
#include "Core/Time/FrameTimer.hpp"
#include "Editor/ImGui/EditorPanels.hpp"
#include "Platform/Input/Input.hpp"
#include "Platform/Window/Window.hpp"
#include "Renderer/Scene/RenderExtractionManager.hpp"
#include "Renderer/Resources/Model.hpp"
#include "Renderer/View/RenderView.hpp"
#include "Renderer/Vulkan/Vulkan.hpp"
#include "Scene/SceneManager.hpp"
#include "Scene/SceneQueries.hpp"

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
    Engine() = default;

    void run()
    {
        glfwWindow = std::make_unique<Window>(WIDTH, HEIGHT, "[Faye] - Vulkan Renderer");

        LOG_INFO(Logger::getInstance(), "Init Vulkan...");

        vkData = std::make_unique<Vulkan>(*glfwWindow);
        modelRegistry = std::make_unique<ModelRegistry>();
        renderExtractionManager = std::make_unique<RenderExtractionManager>();
        sceneManager = std::make_unique<SceneManager>();
        initializeScene();
        editorPanels.setPrimitiveCreateCallback([this](PrimitiveType primitiveType)
                                                { return createPrimitiveEntity(primitiveType); });

        LOG_INFO(Logger::getInstance(), "Starting main loop...");
        mainLoop();
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
    std::unique_ptr<RenderExtractionManager> renderExtractionManager;
    std::unique_ptr<SceneManager> sceneManager;
    FrameTimer timer;
    std::array<ModelHandle, static_cast<size_t>(PrimitiveType::Count)> primitiveModelHandles{};
    Entity activeCameraEntity;
    bool editorViewportHovered = true;
    bool editorViewportFocused = true;

    ModelHandle ensurePrimitiveHandle(PrimitiveType primitiveType)
    {
        ModelHandle &handle = primitiveModelHandles[primitiveIndex(primitiveType)];
        if (!handle.isValid())
        {
            handle = modelRegistry->registerModel(Model::createPrimitive(*vkData->getVkDevice(), primitiveType));
        }

        return handle;
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
        mesh.color = {1.0f, 1.0f, 1.0f};
        return entity;
    }

    void initializeScene()
    {
        Scene &scene = sceneManager->createScene("Main Scene");

        Entity editorCamera = scene.createEntity("Editor Camera");
        activeCameraEntity = editorCamera;
        editorCamera.addTransform();
        editorCamera.addCamera(true);

        Entity meshEntity = scene.createEntity("Cube A");
        auto &meshTransform = meshEntity.addTransform();
        auto meshHandle = ensurePrimitiveHandle(PrimitiveType::Cube);
        auto &meshComponent = meshEntity.addMesh(meshHandle);
        meshTransform.translation = {-1.f, 0.f, -1.f};
        meshTransform.rotation = glm::vec3(45.f, 0.f, 0.f);
        meshTransform.scale = {.5f, .5f, .5f};
        meshComponent.color = {1.f, 1.f, 1.f};

        Entity secondMeshEntity = scene.createEntity("Cube B");
        auto &secondMeshTransform = secondMeshEntity.addTransform();
        auto secondMeshHandle = ensurePrimitiveHandle(PrimitiveType::Cube);
        auto &secondMeshComponent = secondMeshEntity.addMesh(secondMeshHandle);
        secondMeshTransform.translation = {2.f, 0.f, -1.f};
        secondMeshTransform.rotation = glm::vec3(45.f, 0.f, 0.f);
        secondMeshTransform.scale = {.5f, .5f, .5f};
        secondMeshComponent.color = {1.f, 1.f, 1.f};

        Entity floorEntity = scene.createEntity("Floor");
        auto &floorTransform = floorEntity.addTransform();
        auto floorHandle = ensurePrimitiveHandle(PrimitiveType::Plane);
        auto &floorMeshComponent = floorEntity.addMesh(floorHandle);
        floorTransform.translation = {0.f, -0.5f, 0.f};
        floorTransform.scale = {3.f, 1.f, 3.f};
        floorMeshComponent.color = {0.65f, 0.65f, 0.65f};

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
                RenderDebugMode::Lit};

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

            RenderSceneSnapshot renderScene = renderExtractionManager->extract(scene, *modelRegistry);

            VulkanFrameInput frameInput{
                renderView,
                renderScene,
                static_cast<int>(timer.getFrameTime(1)),
                static_cast<int>(timer.getAverageFPS())};

            vkData->renderFrame(frameInput, [this, &scene, cameraComponent](ImGuiFrameData &frameData)
                                {
                                    editorPanels.draw(frameData);

                                    if (frameData.sceneViewportClicked)
                                    {
                                        const auto hit = raycastScene(
                                            scene,
                                            *modelRegistry,
                                            cameraComponent->camera,
                                            {frameData.sceneViewportClickUv.x, frameData.sceneViewportClickUv.y});
                                        editorPanels.setSelectedEntity(hit.has_value() ? scene.getEntity(hit->entity) : Entity{});
                                    }

                                    editorViewportHovered = frameData.sceneViewportHovered;
                                    editorViewportFocused = frameData.sceneViewportFocused; });
        }
    }
};