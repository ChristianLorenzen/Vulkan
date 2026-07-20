#include "Engine.hpp"
#include "Scene/SceneManager.hpp"


namespace Faye {

    void Engine::initialize() {
        Time::StopWatch initRunTimer;

        glfwWindow = std::make_unique<Window>(WIDTH, HEIGHT, "[Faye] - Vulkan Renderer");

        LOG_INFO(Logger::get(), "Init Vulkan...");

        vkData = std::make_unique<Vulkan>(*glfwWindow);
        modelRegistry = std::make_unique<ModelRegistry>(*vkData->getVkDevice());
        materialRegistry = std::make_unique<MaterialRegistry>();

        // Constructed on the main thread: the JobSystem captures this thread's
        // id as the main-thread affinity for scheduleMainThread/pumpMainThread.
        jobSystem = std::make_unique<Jobs::JobSystem>();
        frameContext.jobs = jobSystem.get();
        LOG_INFO(Logger::get(), "Job system started with {} worker threads",
                 Jobs::hardwareThreadsMinusOne());

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

        LOG_INFO(Logger::get(), "Initialization complete. Time taken: {} ms", initRunTimer.elapsedMs());

        LOG_INFO(Logger::get(), "Starting main loop...");
    }

    float Engine::tick() {
        frameContext.dt = static_cast<float>(timer->getDeltaTimeS());
        update();
        // Drain jobs pinned to the main thread (Vulkan/GLFW/ImGui work
        // scheduled from workers) once per frame.
        jobSystem->pumpMainThread();
        return frameContext.dt;
    }

    void Engine::renderSceneInto(const Vulkan::FrameToken& token, const RenderView& view) {
        RenderSceneSnapshot snapshot = sceneManager->buildRenderSnapshot();
        VulkanFrameInput input{
            view, snapshot,
            postProcessSettingsEntity.tryGetPostProcessStack(),
            frameTimeMs(), averageFps()
        };
        vkData->renderScene(token, input);
    }

    void Engine::compositeToSwapchain(const Vulkan::FrameToken& token) {
        vkData->compositeSceneToSwapchain(token, postProcessSettingsEntity.tryGetPostProcessStack());
    }

    std::optional<uint32_t> Engine::finalPostProcessTarget() const {
        return vkData->finalPostProcessTarget(postProcessSettingsEntity.tryGetPostProcessStack());
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