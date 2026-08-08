#pragma once

#include "engine/Runtime/Engine.hpp"
#include "Editor/Panels/EditorPanels.hpp"
#include "Editor/ImGuiIntegration/EditorRenderLayer.hpp"
#include "Editor/Utility/EditorCameraController.hpp"
#include "engine/Scene/SceneQueries.hpp"
#include <filesystem>

namespace Faye::Editor {
    class Application {
        public:
            void run();

            // Startup scene to load instead of the default one (--scene / FAYE_SCENE).
            void setStartupScenePath(std::string path) { startupScenePath = std::move(path); }

        private:
            struct ViewportState {
                bool hovered = true, focused = true;
                RenderDebugMode debugMode = RenderDebugMode::Lit;
                // Persistent grid settings for the editor viewport. Enabled by
                // default because the grid's whole purpose is orientation while
                // navigating. The runtime never sees this — it is copied into
                // the editor's RenderView only, in buildRenderView().
                EditorGridSettings grid{ .enabled = true };
            };

            void wirePanels();
            void updateEditorCamera(float dt);
            RenderView buildRenderView();
            void renderFrame(const RenderView& view);
            void onPresent(ImGuiFrameData& frameData);
            void processPendingFileAction();

            Engine            engine;
            Panels::EditorPanels      panels;
            ImGuiIntegration::EditorRenderLayer layer;
            EditorCameraController    editorCamera;
            ViewportState     viewport;

            // Offscreen scene size requested by the viewport panel last frame,
            // applied at the start of the next frame (0 == no pending resize).
            uint32_t pendingViewportWidth = 0;
            uint32_t pendingViewportHeight = 0;

            std::string startupScenePath;

            // File menu request captured during the ImGui pass and applied at
            // the END of the current frame (after endFrame), so scene
            // references are never invalidated mid-frame.
            struct PendingFileAction {
                bool pending = false;
                Panels::FileAction action = Panels::FileAction::New;
                std::string path;
            };
            PendingFileAction pendingFileAction;
    };
}