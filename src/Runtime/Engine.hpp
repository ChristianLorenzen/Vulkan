#pragma once

#include <concepts>
#include <memory>
#include <optional>
#include <vector>
#include <exception>
#include <filesystem>

#include "Assets/AssetDatabase.hpp"
#include "Assets/ModelRegistry.hpp"
#include "Core/EngineContext.hpp"
#include "Core/ITick.hpp"
#include "Core/HotReload/HotReloadManager.hpp"
#include "Core/HotReload/HotReloadSystem.hpp"
#include "Core/Logging/Logger.hpp"
#include "Core/Path/Paths.hpp"
#include "Core/Time/Timer.hpp"
#include "Core/Time/FixedStepper.hpp"
#include "Core/Jobs/JobSystem.hpp"
#include "Platform/Input/Input.hpp"
#include "Platform/Window/Window.hpp"
#include "Renderer/Scene/RenderExtractionManager.hpp"
#include "Renderer/View/RenderView.hpp"
#include "Renderer/Vulkan/Vulkan.hpp"
#include "Renderer/Vulkan/vk_shader_manager.hpp"
#include "Scene/SceneBuilder.hpp"
#include "Scene/SceneManager.hpp"
#include "Scripting/ScriptingSystem.hpp"
#include "quill/LogMacros.h"

#include <glm/glm.hpp>


const uint32_t WIDTH = 2560;
const uint32_t HEIGHT = 1440;

namespace Faye {
    class Engine
    {
    public:
        Engine() = default;

        Jobs::JobHandle initialize();

        void pollEvents();
        bool shouldClose() const;
        float tick();

        // --- per-frame rendering: the editor drives the renderer phases and
        //     interleaves its UI layer; these record the engine-owned scene. ---
        // Build the frame input (scene snapshot + post-process stack + timing)
        // and record the depth prepass, scene pass, lights and PP effects.
        void renderSceneInto(const Vulkan::FrameToken& token, const RenderView& view);
        // Composite the final post-processed scene into the swapchain image.
        void compositeToSwapchain(const Vulkan::FrameToken& token);
        // Post-process target holding the final image for the active stack, or
        // nullopt when no effects are enabled (viewport shows raw scene color).
        std::optional<uint32_t> finalPostProcessTarget() const;

        Vulkan&             renderer()          { return *vkData; }
        int                 frameTimeMs() const { return static_cast<int>(timer->getDeltaTimeMS()); }
        int                 averageFps() const;
        std::vector<Profiler::ResolvedScope>    getFrameScopeData() { return vkData->getScopeData(); }

        GLFWwindow*         window();
        VkExtent2D          windowExtent()      { return glfwWindow->getExtent(); }
        Scene&              activeScene();
        Entity              activeCamera()      { return activeCameraEntity; }
        ModelRegistry&      models()            { return *modelRegistry; }
        MaterialRegistry&   materials()         { return *materialRegistry; }
        AssetDatabase&      assets()            { return *assetDatabase; }
        ScriptSystem&       scripts()           { return scriptingSystem->getScriptSystem(); }
        Jobs::JobSystem&    jobs()              { return *jobSystem; }
        HotReloadSystem&    reloadSystem()      { return *hotReloadSystem; }
        Entity              createPrimitive(PrimitiveType t) { return sceneBuilder->createPrimitiveEntity(activeScene(), t); }
        Entity              importAndCreateModel(std::filesystem::path path) { return sceneBuilder->importAndCreateModel(activeScene(), std::move(path)); }
        VkExtent2D          sceneRenderExtent() const { return vkData->getSceneRenderExtent(); }

        // ---- scene files -------------------------------------------------
        // Startup scene to load on initialize() instead of the default scene.
        void setStartupScenePath(std::string path) { startupScenePath = std::move(path); }
        const std::string &currentScenePath() const { return sceneManager->currentScenePath(); }

        // Editor actions (deferred by the editor to end-of-frame).
        SceneBuilder::SceneSetup newScene();
        std::string saveScene(const std::string &path);          // "" = success
        std::string saveSceneAs(const std::string &path);        // "" = success; forks the scene uuid
        SceneFileLoadResult loadScene(const std::string &path);

    private:
        // Register an ITick coordinator. These are the engine's main-thread,
        // per-frame coordination systems (timing, input, hot-reload, scripting,
        // scene management) — inherently serial, so they run in registration
        // order in a plain loop. Parallel ECS work lives behind ISystem /
        // SystemSchedule (see RenderExtractionManager), not here.
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
        void fixedUpdate() { for (auto &system : systems) { system->OnFixedUpdate(fixedContext); } }
        void stop() { for (auto &system : systems) { system->OnStop(); } }

        // ---------------------------------------------------------------------
        //   destroy first  -> systems      (scripts die before what they captured)
        //                  -> sceneBuilder (captured by scripts; now safe)
        //                  -> registries   (referenced by scripts/builder)
        //   destroy last   -> vkData/glfwWindow (device alive through GPU teardown)
        //   (editor panels live in the Editor now, not the Engine)
        // ---------------------------------------------------------------------

        // Window + renderer. Declared first -> destroyed last, so the Vulkan device
        // outlives everything that releases GPU resources during teardown.
        std::unique_ptr<Window> glfwWindow;
        std::unique_ptr<Vulkan> vkData;

        // Job system. Declared before the systems so it is destroyed after
        // them: every system's teardown (e.g. HotReloadManager::stop draining
        // its in-flight scan) can still rely on the worker pool being alive.
        std::unique_ptr<Jobs::JobSystem> jobSystem;

        // Asset registries.
        std::unique_ptr<ModelRegistry> modelRegistry;
        std::unique_ptr<MaterialRegistry> materialRegistry;
        std::unique_ptr<AssetDatabase> assetDatabase;
        std::string startupScenePath;

        // Scene content coordinator.
        std::unique_ptr<SceneBuilder> sceneBuilder;

        // ITick systems — registration order == execution order.
        std::vector<std::unique_ptr<ITick>> systems;

        // Non-owning views into `systems`.
        Time::Timer *timer = nullptr;
        HotReloadSystem *hotReloadSystem = nullptr;
        SceneManager *sceneManager = nullptr;
        ScriptingSystem *scriptingSystem = nullptr;

        // Fixed-timestep accumulator driving OnFixedUpdate at a constant rate.
        FixedStepper fixedStepper{1.0 / 60.0};

        VulkanShaderManager shaderManager;

        // Per-tick contexts pushed to every system: variable frame delta for the
        // Update stage, the fixed step for FixedUpdate.
        EngineContext frameContext;
        EngineContext fixedContext;

        // Existing fields
        Entity activeCameraEntity;
        Entity postProcessSettingsEntity;

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
    };
}