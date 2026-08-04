#include "Editor/Panels/EditorPanels.hpp"

#include "Core/Logging/Logger.hpp"
#include "Editor/Panels/AssetExplorer/AssetExplorerPanel.hpp"
#include "Editor/Panels/Console/ConsolePanel.hpp"
#include "Editor/Panels/EditorView/EditorViewPanel.hpp"
#include "Editor/Panels/FrameStats/FrameStatsPanel.hpp"
#include "Editor/Panels/Hierarchy/HierarchyPanel.hpp"
#include "Editor/Panels/Inspector/InspectorPanel.hpp"
#include "Editor/Panels/RuntimeView/RuntimeViewPanel.hpp"
#include "Editor/Panels/WaterDebug/WaterDebugPanel.hpp"
#include "Editor/Panels/SceneSettings/SceneSettingsPanel.hpp"

#include "imgui.h"

#include <string>

namespace Faye::Editor::Panels
{
    EditorPanels::EditorPanels()
    {
        panels.push_back(std::make_unique<ConsolePanel>(Logger::getConsoleSink()));
        panels.push_back(std::make_unique<FrameStatsPanel>());
        panels.push_back(std::make_unique<EditorViewPanel>());
        panels.push_back(std::make_unique<HierarchyPanel>());
        panels.push_back(std::make_unique<InspectorPanel>());
        panels.push_back(std::make_unique<RuntimeViewPanel>());
        panels.push_back(std::make_unique<AssetExplorerPanel>());
        panels.push_back(std::make_unique<WaterDebugPanel>());
        panels.push_back(std::make_unique<SceneSettingsPanel>());

        // One icon upload shared by every file view (asset explorer grid,
        // inspector texture picker).
        for (const auto &panel : panels)
        {
            panel->setIconLibrary(&icons);
        }
    }

    EditorPanels::~EditorPanels() = default;

    void EditorPanels::draw(ImGuiFrameData &frameData)
    {
        drawDockspace();

        for (const auto &panel : panels)
        {
            if (!panel->isOpen())
                continue;

            panel->draw(frameData, boundScene, selectedEntity, selectedMeshNodeIndex, materialRegistry,
                        modelRegistry, &textureThumbnailCallback, materialTemplateRegistry);
        }
    }

    void EditorPanels::drawDockspace()
    {
        // Pin the dockspace host window to the full OS window so the entire
        // application surface becomes the editor UI.
        const ImGuiViewport *viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);

        ImGuiWindowFlags hostFlags =
            ImGuiWindowFlags_MenuBar |
            ImGuiWindowFlags_NoDocking |
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoBringToFrontOnFocus |
            ImGuiWindowFlags_NoNavFocus;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("##DockspaceRoot", nullptr, hostFlags);
        ImGui::PopStyleVar(3);

        ImGui::DockSpace(ImGui::GetID("MainDockSpace"), ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

        // TODO: maybe move global style vars to a central location
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(16.0f, 4.0f));

        if (ImGui::BeginMenuBar())
        {
            drawFileMenu();

            if (ImGui::BeginMenu("View"))
            {
                for (const auto &panel : panels)
                {
                    if (!panel->showInViewMenu())
                        continue;

                    bool open = panel->isOpen();
                    if (ImGui::MenuItem(panel->getName(), nullptr, &open))
                        panel->setOpen(open);
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Add"))
            {
                if (ImGui::MenuItem("Entity"))
                {
                    // TODO: Might need to do this outside of ImGui context.
                    this->boundScene->createEntity({"New Entity"}).add<TransformComponent>();
                }
                if (ImGui::BeginMenu("Primitives"))
                {
                    drawPrimitiveMenuItem(PrimitiveType::Cube);
                    drawPrimitiveMenuItem(PrimitiveType::Sphere);
                    drawPrimitiveMenuItem(PrimitiveType::Plane);
                    drawPrimitiveMenuItem(PrimitiveType::Capsule);
                    drawPrimitiveMenuItem(PrimitiveType::WaterPlane);
                    ImGui::EndMenu();
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
            ImGui::PopStyleVar();
        }

        drawFilePicker();

        ImGui::End();
    }

    void EditorPanels::drawFileMenu()
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("New"))
            {
                if (fileActionCallback)
                    fileActionCallback(FileAction::New, {});
            }
            if (ImGui::MenuItem("Save"))
            {
                if (fileActionCallback)
                    fileActionCallback(FileAction::Save, {});
            }
            if (ImGui::MenuItem("Save As..."))
            {
                filePicker.open(Widgets::FilePickerDialog::Mode::Save,
                                {".faye"},
                                Paths::projects() / "scenes");
            }
            if (ImGui::MenuItem("Open..."))
            {
                filePicker.open(Widgets::FilePickerDialog::Mode::Open,
                                {".faye"},
                                Paths::projects() / "scenes");
            }
            ImGui::EndMenu();
        }
    }

    void EditorPanels::drawFilePicker()
    {
        if (!filePicker.draw(icons))
        {
            return;
        }
        if (!fileActionCallback)
        {
            return;
        }
        if (filePicker.mode() == Widgets::FilePickerDialog::Mode::Open)
        {
            fileActionCallback(FileAction::Open, filePicker.path());
        }
        else
        {
            fileActionCallback(FileAction::SaveAs, filePicker.savePath());
        }
    }

    void EditorPanels::drawPrimitiveMenuItem(PrimitiveType primitiveType)
    {
        const std::string menuLabel = std::string("Add ") + std::string(primitiveTypeName(primitiveType));
        if (!ImGui::MenuItem(menuLabel.c_str()))
        {
            return;
        }

        if (!primitiveCreateCallback)
        {
            return;
        }

        Entity createdEntity = primitiveCreateCallback(primitiveType);
        if (createdEntity.isValid())
        {
            selectedEntity = createdEntity;
        }
    }
}
