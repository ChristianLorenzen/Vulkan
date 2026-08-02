#include "Engine.hpp"
#include "Scene/SceneManager.hpp"


namespace Faye {

    Jobs::JobHandle Engine::initialize() {
        Time::StopWatch initRunTimer;

        glfwWindow = std::make_unique<Window>(WIDTH, HEIGHT, "[Faye] - Vulkan Renderer");

        LOG_INFO(Logger::get(), "Init Vulkan...");

        vkData = std::make_unique<Vulkan>(*glfwWindow);
        assetDatabase = std::make_unique<AssetDatabase>();
        modelRegistry = std::make_unique<ModelRegistry>(*vkData->getVkDevice(), *assetDatabase);
        materialRegistry = std::make_unique<MaterialRegistry>(*assetDatabase);

        // Constructed on the main thread: the JobSystem captures this thread's
        // id as the main-thread affinity for scheduleMainThread/pumpMainThread.
        jobSystem = std::make_unique<Jobs::JobSystem>();
        frameContext.jobs = jobSystem.get();
        fixedContext.jobs = jobSystem.get();
        LOG_INFO(Logger::get(), "Job system started with {} worker threads",
                 Jobs::hardwareThreadsMinusOne());

        // Adding systems — registration order == tick order.
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
            *modelRegistry, *materialRegistry, *assetDatabase,
            scriptingSystem->getScriptSystem(), scriptingSystem->getLuaScriptSystem(), *jobSystem);
        
        Jobs::JobHandle handle = jobSystem->schedule([this]() {
            SceneBuilder::SceneSetup sceneSetup = sceneBuilder->populate(sceneManager->getActiveScene());
            activeCameraEntity = sceneSetup.activeCamera;
            postProcessSettingsEntity = sceneSetup.postProcessSettings;
        });

        LOG_INFO(Logger::get(), "Initialization complete. Time taken: {} ms", initRunTimer.elapsedMs());

        LOG_INFO(Logger::get(), "Starting main loop...");

        return handle;
    }

    float Engine::tick() {
        frameContext.dt = static_cast<float>(timer->getDeltaTimeS());

        // Fixed timestep: run OnFixedUpdate 0..N times so fixed-step work
        // advances at a constant rate regardless of frame rate. dt is ALWAYS the
        // fixed step (determinism). No system does fixed work yet, so this is a
        // no-op today — but OnFixedUpdate is finally wired.
        const int fixedTicks = fixedStepper.advance(frameContext.dt);
        fixedContext.dt = static_cast<float>(fixedStepper.stepSeconds());
        for (int i = 0; i < fixedTicks; ++i)
            fixedUpdate();

        update();

        // Drain jobs pinned to the main thread (Vulkan/GLFW/ImGui work
        // scheduled from workers) once per frame.
        jobSystem->pumpMainThread();
        return frameContext.dt;
    }

    void Engine::renderSceneInto(const Vulkan::FrameToken& token, const RenderView& view) {
        const RenderSceneSnapshot &snapshot = sceneManager->buildRenderSnapshot(*jobSystem);
        VulkanFrameInput input{
            view, snapshot,
            postProcessSettingsEntity.tryGet<PostProcessStackComponent>(),
            frameTimeMs(), averageFps()
        };
        vkData->renderScene(token, input);
    }

    void Engine::compositeToSwapchain(const Vulkan::FrameToken& token) {
        vkData->compositeSceneToSwapchain(token, postProcessSettingsEntity.tryGet<PostProcessStackComponent>());
    }

    std::optional<uint32_t> Engine::finalPostProcessTarget() const {
        return vkData->finalPostProcessTarget(postProcessSettingsEntity.tryGet<PostProcessStackComponent>());
    }

    int Engine::averageFps() const {
        const double dtS = timer->getDeltaTimeS();
        return dtS > 0.0 ? static_cast<int>(1.0 / dtS) : 0;
    }

    void Engine::pollEvents() { glfwPollEvents(); }

    bool Engine::shouldClose() const { return glfwWindow->shouldClose(); }

    GLFWwindow* Engine::window() { return glfwWindow->getWindow(); }

    Scene& Engine::activeScene() { return sceneManager->getActiveScene(); }
}