#include "Editor.hpp"

namespace Faye {

    void Editor::run()
    {
        engine.initialize();
        layer.init(engine.window(), engine.renderer().targets());
        wirePanels();
        panels.bindScene(&engine.activeScene());

        while (!engine.shouldClose()) {
            engine.pollEvents();
            const float dt = engine.tick();

            updateEditorCamera(dt);

            RenderView view = buildRenderView();

            renderFrame(view);
        }
    }

    void Editor::wirePanels() {
        panels.setMaterialRegistry(&engine.materials());
        panels.setModelRegistry(&engine.models());
        panels.setScriptSystem(&engine.scripts());
        panels.setPrimitiveCreateCallback([this](PrimitiveType t){ return engine.createPrimitive(t); });
        panels.setTextureThumbnailCallback([this](MaterialHandle h, TextureType t) -> ImTextureID {
            const Material* m = engine.materials().getMaterial(h);
            if (!m) return 0;
            const MaterialTextureView tex = engine.renderer().getMaterialTexture(h, *m, t);
            return layer.registerThumbnail(tex.imageView, tex.sampler);
        });
    }

    void Editor::renderFrame(const RenderView& view) {
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
        }

        engine.renderSceneInto(*frame, view);

        r.beginPresentPass(*frame);
        engine.compositeToSwapchain(*frame);

        layer.beginFrame();

        ImGuiFrameData frameData{};
        frameData.frameTimeMs = engine.frameTimeMs();
        frameData.averageFps = engine.averageFps();
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

    void Editor::updateEditorCamera(float dt) {
        auto* tf  = engine.activeCamera().tryGetTransform();
        auto* cam = engine.activeCamera().tryGetCamera();
        if (!tf || !cam) throw std::runtime_error("Active scene camera is not configured correctly");

        cam->camera.saveViewProjectionMatrix();
        Input& input = Input::getInstance();
        input.updateEditorCamera(engine.window(), *tf, dt, {viewport.hovered, viewport.focused});
        cam->camera.setViewYXZ(tf->translation, tf->rotation);
        if (input.isKeyPressed(engine.window(), input.keyMap.escape))
            glfwSetWindowShouldClose(engine.window(), true);
    }

    RenderView Editor::buildRenderView() {
        VkExtent2D ext = engine.sceneRenderExtent();
        // Before the viewport panel sizes the offscreen target (first frame), the
        // scene extent is 0x0; fall back to the window so aspectRatio() never /0.
        if (ext.width == 0 || ext.height == 0) ext = engine.windowExtent();
        auto* cam = engine.activeCamera().tryGetCamera();
        RenderView view{ &cam->camera, {ext.width, ext.height},
                        RenderOutputTarget::OffscreenSceneColor, viewport.debugMode };
        cam->camera.setPerspectiveProjection(glm::radians(50.f), view.viewport.aspectRatio(), 0.1f, 100.f);
        return view;
    }

    void Editor::onPresent(ImGuiFrameData& frameData) {
        panels.draw(frameData);
        if (frameData.viewportClicked) {
            auto* cam = engine.activeCamera().tryGetCamera();
            const auto hit = raycastScene(engine.activeScene(), engine.models(), cam->camera,
                                        {frameData.viewportClickUv.x, frameData.viewportClickUv.y});
            panels.setSelectedEntity(hit ? engine.activeScene().getEntity(hit->entity) : Entity{});
        }
        viewport.hovered   = frameData.viewportHovered;
        viewport.focused   = frameData.viewportFocused;
        viewport.debugMode = frameData.viewportDebugMode;
    }
}