#include "Editor.hpp"

#include "Core/Logging/Logger.hpp"
#include "Editor/Panels/AssetExplorer/AssetExplorerPanel.hpp"
#include "Renderer/Material/TextureLoader.hpp"

#include "quill/LogMacros.h"

namespace Faye::Editor {

    void Application::run()
    {
        Jobs::JobHandle handle = engine.initialize();
        layer.init(engine.window(), engine.renderer().targets());
        wirePanels();
        panels.bindScene(&engine.activeScene());

        // Wait for scene to be populated.
        engine.jobs().wait(handle);

        while (!engine.shouldClose()) {
            engine.pollEvents();
            const float dt = engine.tick();

            updateEditorCamera(dt);

            RenderView view = buildRenderView();

            renderFrame(view);
        }
    }

    void Application::wirePanels() {
        Panels::AssetExplorerPanel *assetPanel = panels.getPanelByType<Panels::AssetExplorerPanel>();
        engine.reloadSystem().getHotReloadManager().subscribe([assetPanel](const HotReloadEvent &event) {
            assetPanel->FileChangeCallback(event);
        }, {"project-files"});
        assetPanel->setEntityCreateCallback([this](const std::filesystem::path &path) {
            return engine.importAndCreateModel(path);
        });
        assetPanel->setInitialFileWatch(engine.reloadSystem().getHotReloadManager().getWatch("project-files"));

        panels.setIconTextureCallback([this](const std::filesystem::path &path) -> ImTextureID {
            try {
                const MaterialTextureView tex = engine.renderer().getOrCreateTexture(
                    loadTextureFromFile(path.string(), TextureType::Albedo));
                return layer.registerThumbnail(tex.imageView, tex.sampler);
            } catch (const std::exception &e) {
                LOG_WARNING(Logger::get(), "Editor icon load failed for {}: {}", path.string(), e.what());
                return 0;
            }
        });
        panels.loadIcons();

        panels.setMaterialRegistry(&engine.materials());
        panels.setModelRegistry(&engine.models());
        panels.setPrimitiveCreateCallback([this](PrimitiveType t){ return engine.createPrimitive(t); });
        panels.setTextureThumbnailCallback([this](MaterialHandle h, TextureType t) -> ImTextureID {
            const Material* m = engine.materials().getMaterial(h);
            if (!m) return 0;
            const MaterialTextureView tex = engine.renderer().getMaterialTexture(h, *m, t);
            return layer.registerThumbnail(tex.imageView, tex.sampler);
        });
    }

    void Application::renderFrame(const RenderView& view) {
        Vulkan& r = engine.renderer();

        // Apply the offscreen resize the viewport panel requested last frame.
        // Re-register the ImGui viewport textures if the resize actually took effect.
        if (pendingViewportWidth > 0 && pendingViewportHeight > 0) {
            if (r.setSceneRenderSize(pendingViewportWidth, pendingViewportHeight)) {
                layer.onSceneResized(r.targets());
            }
            pendingViewportWidth = 0;
            pendingViewportHeight = 0;
        }

        auto frame = r.beginFrame();
        if (!frame) {
            return;
        }

        // Swapchain recreated (e.g. OS window resize): reinit ImGui for the new
        // swapchain before recording any UI.
        if (frame->swapchainRecreated) {
            layer.onSwapchainRecreated(engine.window(), r.targets());
            // The layer dropped its thumbnail descriptors, so the cached icon
            // texture ids are stale — re-register them.
            panels.loadIcons();
        }

        engine.renderSceneInto(*frame, view);

        r.beginPresentPass(*frame);
        engine.compositeToSwapchain(*frame);

        layer.beginFrame();

        ImGuiFrameData frameData{};
        frameData.frameTimeMs = engine.frameTimeMs();
        frameData.averageFps = engine.averageFps();
        frameData.viewportGrid = viewport.grid;
        layer.buildViewportFrameData(r.targets(),
                                     static_cast<uint32_t>(frame->frameIndex),
                                     viewport.debugMode,
                                     engine.finalPostProcessTarget(),
                                     frameData);

        onPresent(frameData);

        // Remember the size the panel requested this frame; applied next frame.
        if (frameData.requestedViewportSize.x > 0.0f && frameData.requestedViewportSize.y > 0.0f) {
            pendingViewportWidth = static_cast<uint32_t>(frameData.requestedViewportSize.x);
            pendingViewportHeight = static_cast<uint32_t>(frameData.requestedViewportSize.y);
        }

        layer.render(frame->cmd);

        r.endPresentPass(*frame);
        r.endFrame(*frame);
    }

    void Application::updateEditorCamera(float dt) {
        auto* tf  = engine.activeCamera().tryGet<TransformComponent>();
        auto* cam = engine.activeCamera().tryGet<CameraComponent>();
        if (!tf || !cam) return; // throw std::runtime_error("Active scene camera is not configured correctly");

        cam->camera.saveViewProjectionMatrix();
        Input& input = Input::getInstance();
        input.updateEditorCamera(engine.window(), *tf, dt, {viewport.hovered, viewport.focused});
        cam->camera.setViewYXZ(tf->translation, tf->rotation);
        if (input.isKeyPressed(engine.window(), input.keyMap.escape))
            glfwSetWindowShouldClose(engine.window(), true);
    }

    RenderView Application::buildRenderView() {
        VkExtent2D ext = engine.sceneRenderExtent();
        // Before the viewport panel sizes the offscreen target (first frame), the
        // scene extent is 0x0; fall back to the window so aspectRatio() never /0.
        if (ext.width == 0 || ext.height == 0) ext = engine.windowExtent();
        auto* cam = engine.activeCamera().tryGet<CameraComponent>();
        RenderView view{ &cam->camera, {ext.width, ext.height},
                        RenderOutputTarget::OffscreenSceneColor, viewport.debugMode,
                        viewport.grid };
        cam->camera.setPerspectiveProjection(glm::radians(50.f), view.viewport.aspectRatio(), 0.1f, 100.f);
        return view;
    }

    void Application::onPresent(ImGuiFrameData& frameData) {
        panels.draw(frameData);
        if (frameData.viewportClicked) {
            auto* cam = engine.activeCamera().tryGet<CameraComponent>();
            const auto hit = raycastScene(engine.activeScene(), engine.models(), cam->camera,
                                        {frameData.viewportClickUv.x, frameData.viewportClickUv.y});
            panels.setSelectedEntity(hit ? engine.activeScene().getEntity(hit->entity) : Entity{});
        }
        viewport.hovered   = frameData.viewportHovered;
        viewport.focused   = frameData.viewportFocused;
        viewport.debugMode = frameData.viewportDebugMode;
        viewport.grid       = frameData.viewportGrid;
    }
}