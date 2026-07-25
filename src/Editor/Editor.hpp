#pragma once

#include "Runtime/Engine.hpp"
#include "Editor/ImGui/EditorPanels.hpp"
#include "Editor/ImGui/EditorRenderLayer.hpp"
#include "Scene/SceneQueries.hpp"

namespace Faye {
    class Editor {
        public:
            void run();

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

            Engine            engine;
            EditorPanels      panels;
            EditorRenderLayer layer;
            ViewportState     viewport;

            // Offscreen scene size requested by the viewport panel last frame,
            // applied at the start of the next frame (0 == no pending resize).
            uint32_t pendingViewportWidth = 0;
            uint32_t pendingViewportHeight = 0;
    };
}