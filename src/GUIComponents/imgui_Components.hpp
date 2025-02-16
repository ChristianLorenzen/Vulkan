#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_vulkan.h"

#include "quill/Frontend.h"
#include "quill/LogMacros.h"
#include "quill/Logger.h"

namespace Faye {
    class Components {
        public:
        static void FrameCounter(quill::Logger* logger, int frametime, int fps) {
            ImGuiIO& io = ImGui::GetIO();
            ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;
            
            const float PAD = 10.0f;
            const ImGuiViewport* viewport = ImGui::GetMainViewport();
            ImVec2 work_pos = viewport->WorkPos; // Use work area to avoid menu-bar/task-bar, if any!
            ImVec2 work_size = viewport->WorkSize;
            ImVec2 window_pos, window_pos_pivot;

            window_pos.x = (work_pos.x + work_size.x - PAD);
            window_pos.y = work_pos.y + work_size.y - PAD; // (work_pos.y + work_size.y - PAD);
            window_pos_pivot.x = 1.0f;
            window_pos_pivot.y = 1.0f;
            ImGui::SetNextWindowPos(window_pos, ImGuiCond_Always, window_pos_pivot);
            ImGui::SetNextWindowViewport(viewport->ID);
            window_flags |= ImGuiWindowFlags_NoMove;

            ImGui::SetNextWindowBgAlpha(0.35f); // Transparent background

            if (ImGui::Begin("Frame Counter", nullptr, window_flags)) {
                ImGui::Text("Frame statistics:");
                ImGui::Separator();
                ImGui::Text("Frame Time: %d ms", frametime);
                ImGui::Text("FPS: %d", fps);
            }

            ImGui::End();
        }

        static void CreateDockspace() {
            if (ImGui::Begin("Dockspace", nullptr)) { 
                ImGui::DockSpace(ImGui::GetID("MyDockSpace"), ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);
            }

            ImGui::End();
        }
    };
}