#pragma once

#include <array>
#include <concepts>
#include <memory>
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <exception>
#include <iostream>
#include <fstream>
#include <algorithm>

#include "Assets/ModelRegistry.hpp"
#include "Core/EngineContext.hpp"
#include "Core/ITick.hpp"
#include "Core/HotReload/HotReloadManager.hpp"
#include "Core/HotReload/HotReloadSystem.hpp"
#include "Core/Logging/Logger.hpp"
#include "Core/Path/Paths.hpp"
#include "Core/Time/Timer.hpp"
#include "Editor/ImGui/EditorPanels.hpp"
#include "Platform/Input/Input.hpp"
#include "Platform/Window/Window.hpp"
#include "Renderer/Scene/RenderExtractionManager.hpp"
#include "Renderer/View/RenderView.hpp"
#include "Renderer/Vulkan/Vulkan.hpp"
#include "Renderer/Vulkan/vk_shader_manager.hpp"
#include "Scene/SceneBuilder.hpp"
#include "Scene/SceneManager.hpp"
#include "Scene/SceneQueries.hpp"
#include "Scripting/ScriptingSystem.hpp"
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
        Time::StopWatch initRunTimer;

        glfwWindow = std::make_unique<Window>(WIDTH, HEIGHT, "[Faye] - Vulkan Renderer");

        LOG_INFO(Logger::get(), "Init Vulkan...");

        vkData = std::make_unique<Vulkan>(*glfwWindow);
        modelRegistry = std::make_unique<ModelRegistry>(*vkData->getVkDevice());
        materialRegistry = std::make_unique<MaterialRegistry>();

        // Adding systems
        timer = &addSystem<Time::Timer>();
        hotReloadSystem = &addSystem<HotReloadSystem>();
        sceneManager = &addSystem<SceneManager>(*modelRegistry, *materialRegistry);
        scriptingSystem = &addSystem<ScriptingSystem>(*sceneManager, hotReloadSystem->getHotReloadManager());

        hotReloadSystem->getHotReloadManager().subscribe(
            [this](const HotReloadEvent &event)
            { HotReloadShaderCompilation(event); },
            std::vector<std::string_view>{"shader-sources"});

        init();


        postInit();

        sceneBuilder = std::make_unique<SceneBuilder>(
            *modelRegistry, *materialRegistry,
            scriptingSystem->getScriptSystem(), scriptingSystem->getLuaScriptSystem());
        SceneBuilder::SceneSetup sceneSetup = sceneBuilder->populate(sceneManager->getActiveScene());
        activeCameraEntity = sceneSetup.activeCamera;
        postProcessSettingsEntity = sceneSetup.postProcessSettings;

        Time::StopWatch editorPanelSetupTimer;

        editorPanels.setPrimitiveCreateCallback([this](PrimitiveType primitiveType) -> Entity
                                                {
                                                    if (sceneManager == nullptr || !sceneManager->hasActiveScene())
                                                        return {};
                                                    return sceneBuilder->createPrimitiveEntity(sceneManager->getActiveScene(), primitiveType); });
        editorPanels.setMaterialRegistry(materialRegistry.get());
        editorPanels.setModelRegistry(modelRegistry.get());
        editorPanels.setScriptSystem(&scriptingSystem->getScriptSystem());
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
                                                        vkData->getMaterialTextureThumbnail(handle, *material, textureType)); });

        LOG_INFO(Logger::get(), "Editor panels setup complete. Time taken: {} ms", editorPanelSetupTimer.elapsedMs());

        LOG_INFO(Logger::get(), "Initialization complete. Time taken: {} ms", initRunTimer.elapsedMs());

        LOG_INFO(Logger::get(), "Starting main loop...");
        mainLoop();
    }

private:
    template <class T, class... Args>
    T &addSystem(Args &&...args)
    {
        static_assert(std::derived_from<T, ITick>, "T must derive from ITick");
        systems.push_back(std::make_unique<T>(std::forward<Args>(args)...));
        return static_cast<T &>(*systems.back());
    }

    void init() { for (auto &system : systems) { system->OnInit(); } }
    void postInit() { for (auto &system : systems) { system->OnPostInit(); } }
    void update() { for (auto &system : systems) { system->OnUpdate(frameContext); } }
    // Fixed update is a future addition; the contract is already in place.
    // void fixedUpdate() { for (auto &system : systems) { system->OnFixedUpdate(frameContext); } }
    void stop() { for (auto &system : systems) { system->OnStop(); } }

    // ---------------------------------------------------------------------
    //   destroy first  -> editorPanels (drop callbacks)
    //                  -> systems      (scripts die before what they captured)
    //                  -> sceneBuilder (captured by scripts; now safe)
    //                  -> registries   (referenced by scripts/builder)
    //   destroy last   -> vkData/glfwWindow (device alive through GPU teardown)
    // ---------------------------------------------------------------------

    // Window + renderer. Declared first -> destroyed last, so the Vulkan device
    // outlives everything that releases GPU resources during teardown.
    std::unique_ptr<Window> glfwWindow;
    std::unique_ptr<Vulkan> vkData;

    // Asset registries.
    std::unique_ptr<ModelRegistry> modelRegistry;
    std::unique_ptr<MaterialRegistry> materialRegistry;

    // Scene content coordinator.
    std::unique_ptr<SceneBuilder> sceneBuilder;

    // ITick systems — registration order == execution order.
    std::vector<std::unique_ptr<ITick>> systems;

    // Non-owning views into `systems`.
    Time::Timer *timer = nullptr;
    HotReloadSystem *hotReloadSystem = nullptr;
    SceneManager *sceneManager = nullptr;
    ScriptingSystem *scriptingSystem = nullptr;

    // Editor UI. 
    EditorPanels editorPanels;

    VulkanShaderManager shaderManager;

    // Per-tick context pushed to every system
    EngineContext frameContext;

    // Existing fields
    Entity activeCameraEntity;
    Entity postProcessSettingsEntity;
    bool editorViewportHovered = true;
    bool editorViewportFocused = true;
    RenderDebugMode editorViewportDebugMode = RenderDebugMode::Lit;

    void HotReloadShaderCompilation(const HotReloadEvent &event)
    {
        LOG_INFO(Logger::get(), "Hot reload change detected: {}", event.path.filename().string());

        if (event.watchId != "shader-sources" || event.type != HotReloadEventType::Modified)
        {
            LOG_INFO(Logger::get(), "Ignoring hot reload event for watchId '{}' and path '{}'", event.watchId, event.path.string());
            return;
        }

        const std::filesystem::path &path = event.path;
        if (path.extension() == ".vert" || path.extension() == ".frag" || path.extension() == ".comp")
        {
            LOG_INFO(Logger::get(), "Recompiling shader: {}", path.filename().string());
            std::string compileResult = shaderManager.shaderFileChange(path);
            if (!compileResult.empty())
            {
                vkData->notifyShaderRecompilation(compileResult);
            }
        }
    }

    void mainLoop()
    {
        Input &input = Input::getInstance();
        Scene &scene = sceneManager->getActiveScene();
        editorPanels.bindScene(&scene);

        while (!glfwWindow->shouldClose())
        {
            glfwPollEvents();

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
                static_cast<float>(timer->getDeltaTimeS()),
                {editorViewportHovered, editorViewportFocused});
            cameraComponent->camera.setViewYXZ(cameraTransform->translation, cameraTransform->rotation);
            cameraComponent->camera.setPerspectiveProjection(glm::radians(50.f), renderView.viewport.aspectRatio(), 0.1f, 100.f);

            if (input.isKeyPressed(glfwWindow->getWindow(), input.keyMap.escape))
            {
                glfwSetWindowShouldClose(glfwWindow->getWindow(), true);
            }

            // Publish this frame's dt, then tick every system
            frameContext.dt = static_cast<float>(timer->getDeltaTimeS());
            update();

            RenderSceneSnapshot renderScene = sceneManager->buildRenderSnapshot();

            const double frameDeltaS = timer->getDeltaTimeS();
            const int framesPerSecond = frameDeltaS > 0.0 ? static_cast<int>(1.0 / frameDeltaS) : 0;

            VulkanFrameInput frameInput{
                renderView,
                renderScene,
                postProcessSettingsEntity.tryGetPostProcessStack(),
                static_cast<int>(timer->getDeltaTimeMS()),
                framesPerSecond};

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

        // Tear systems down once the main loop ends.
        stop();
    }
};
