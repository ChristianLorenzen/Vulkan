#include "Editor/ImGui/EditorPanels.hpp"

#include "imgui/imgui.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <string>

namespace Faye
{
    namespace
    {
        constexpr ImVec2 kViewportUvMin{0.0f, 1.0f};
        constexpr ImVec2 kViewportUvMax{1.0f, 0.0f};

        void copyNameToBuffer(std::string_view value, std::array<char, 128> &buffer)
        {
            buffer.fill('\0');

            const size_t copyLength = std::min(buffer.size() - 1, value.size());
            std::memcpy(buffer.data(), value.data(), copyLength);
        }

        class FrameStatsPanel final : public IEditorPanel
        {
        public:
            const char *getName() const override { return "Frame Counter"; }
            bool isOpen() const override { return true; }
            void setOpen(bool open) override { alwaysOpen = open; }
            bool showInViewMenu() const override { return false; }

            void draw(ImGuiFrameData &frameData, Scene *scene, Entity &selectedEntity) override
            {
                (void)scene;
                (void)selectedEntity;

                if (!alwaysOpen)
                    return;

                ImGuiWindowFlags windowFlags =
                    ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking |
                    ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
                    ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove;

                const float padding = 10.0f;
                const ImGuiViewport *viewport = ImGui::GetMainViewport();
                ImVec2 workPos = viewport->WorkPos;
                ImVec2 workSize = viewport->WorkSize;

                ImGui::SetNextWindowPos(
                    {workPos.x + workSize.x - padding, workPos.y + workSize.y - padding},
                    ImGuiCond_Always,
                    {1.0f, 1.0f});
                ImGui::SetNextWindowViewport(viewport->ID);
                ImGui::SetNextWindowBgAlpha(0.35f);

                if (ImGui::Begin(getName(), nullptr, windowFlags))
                {
                    ImGui::TextUnformatted("Frame statistics:");
                    ImGui::Separator();
                    ImGui::Text("Frame Time: %d ms", frameData.frameTimeMs);
                    ImGui::Text("FPS: %d", frameData.averageFps);
                }
                ImGui::End();
            }

        private:
            bool alwaysOpen = true;
        };

        // Primary editor viewport — fills its panel and drives the offscreen render resolution.
        class EditorViewPanel final : public IEditorPanel
        {
        public:
            const char *getName() const override { return "Editor View"; }
            bool isOpen() const override { return open; }
            void setOpen(bool isOpen) override { open = isOpen; }

            void draw(ImGuiFrameData &frameData, Scene *scene, Entity &selectedEntity) override
            {
                (void)scene;
                (void)selectedEntity;

                if (!open)
                    return;

                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
                bool visible = ImGui::Begin(getName(), &open, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
                ImGui::PopStyleVar();

                if (visible)
                {
                    frameData.sceneViewportHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);
                    frameData.sceneViewportFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

                    ImVec2 avail = ImGui::GetContentRegionAvail();
                    if (avail.x > 1.0f && avail.y > 1.0f)
                    {
                        // Report desired render size back to the engine for next frame's resize.
                        frameData.requestedSceneViewportSize = avail;

                        if (frameData.sceneViewportTexture != 0)
                        {
                            const ImVec2 imageMin = ImGui::GetCursorScreenPos();
                            ImGui::Image(frameData.sceneViewportTexture, avail, kViewportUvMin, kViewportUvMax);

                            if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
                            {
                                const ImVec2 mousePosition = ImGui::GetMousePos();
                                frameData.sceneViewportClicked = true;
                                frameData.sceneViewportClickUv = ImVec2(
                                    std::clamp((mousePosition.x - imageMin.x) / avail.x, 0.0f, 1.0f),
                                    std::clamp((mousePosition.y - imageMin.y) / avail.y, 0.0f, 1.0f));
                            }
                        }
                    }
                }
                ImGui::End();
            }

        private:
            bool open = true;
        };

        // Secondary viewport showing what the runtime camera sees. Letterboxed to preserve aspect ratio.
        class RuntimeViewPanel final : public IEditorPanel
        {
        public:
            const char *getName() const override { return "Runtime View"; }
            bool isOpen() const override { return open; }
            void setOpen(bool isOpen) override { open = isOpen; }

            void draw(ImGuiFrameData &frameData, Scene *scene, Entity &selectedEntity) override
            {
                (void)scene;
                (void)selectedEntity;

                if (!open)
                    return;

                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
                bool visible = ImGui::Begin(getName(), &open, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
                ImGui::PopStyleVar();

                if (visible)
                {
                    if (frameData.sceneViewportTexture == 0 ||
                        frameData.sceneViewportSize.x <= 0.0f ||
                        frameData.sceneViewportSize.y <= 0.0f)
                    {
                        ImGui::TextUnformatted("Scene image unavailable.");
                    }
                    else
                    {
                        ImVec2 avail = ImGui::GetContentRegionAvail();
                        float srcAspect = frameData.sceneViewportSize.x / frameData.sceneViewportSize.y;
                        ImVec2 imageSize = avail;
                        if (avail.y > 0.0f)
                        {
                            float destAspect = avail.x / avail.y;
                            if (destAspect > srcAspect)
                                imageSize.x = avail.y * srcAspect;
                            else
                                imageSize.y = avail.x / srcAspect;
                        }
                        // Centre the letterboxed image.
                        ImVec2 cursor = ImGui::GetCursorPos();
                        ImGui::SetCursorPos({cursor.x + (avail.x - imageSize.x) * 0.5f,
                                             cursor.y + (avail.y - imageSize.y) * 0.5f});
                        ImGui::Image(frameData.sceneViewportTexture, imageSize, kViewportUvMin, kViewportUvMax);
                    }
                }
                ImGui::End();
            }

        private:
            bool open = false;
        };

        class HierarchyPanel final : public IEditorPanel
        {
        public:
            const char *getName() const override { return "Hierarchy"; }
            bool isOpen() const override { return open; }
            void setOpen(bool isOpen) override { open = isOpen; }

            void draw(ImGuiFrameData &frameData, Scene *scene, Entity &selectedEntity) override
            {
                (void)frameData;

                if (!open)
                    return;

                if (ImGui::Begin(getName(), &open))
                {
                    if (scene == nullptr)
                    {
                        ImGui::TextUnformatted("No active scene.");
                    }
                    else
                    {
                        for (Scene::EntityId entityId : scene->getEntities())
                        {
                            Entity entity = scene->getEntity(entityId);
                            const std::string_view name = entity.getName();
                            const bool isSelected = entity.id() == selectedEntity.id();
                            if (ImGui::Selectable(name.empty() ? "<unnamed>" : name.data(), isSelected))
                            {
                                selectedEntity = entity;
                            }
                        }
                    }
                }
                ImGui::End();
            }

        private:
            bool open = true;
        };

        class InspectorPanel final : public IEditorPanel
        {
        public:
            const char *getName() const override { return "Inspector"; }
            bool isOpen() const override { return open; }
            void setOpen(bool isOpen) override { open = isOpen; }

            void draw(ImGuiFrameData &frameData, Scene *scene, Entity &selectedEntity) override
            {
                (void)frameData;

                if (!open)
                    return;

                if (ImGui::Begin(getName(), &open))
                {
                    if (scene == nullptr || !selectedEntity.isValid())
                    {
                        ImGui::TextUnformatted("No entity selected.");
                    }
                    else
                    {
                        drawEntityMetadata(selectedEntity);
                        drawAttachedComponents(selectedEntity);
                        drawTransform(selectedEntity);
                        drawMesh(selectedEntity);
                        drawCamera(selectedEntity);
                        drawPointLight(selectedEntity);
                    }
                }
                ImGui::End();
            }

        private:
            void drawEntityMetadata(const Entity &entity)
            {
                std::array<char, 128> nameBuffer{};
                copyNameToBuffer(entity.getName(), nameBuffer);

                if (ImGui::InputText("Name", nameBuffer.data(), nameBuffer.size()))
                {
                    entity.setName(nameBuffer.data());
                }

                ImGui::Text("Entity ID: %u", entity.id());
                ImGui::Separator();
            }

            void drawAttachedComponents(const Entity &entity)
            {
                const auto componentKinds = entity.getComponentKinds();
                if (componentKinds.empty())
                {
                    return;
                }

                ImGui::TextUnformatted("Attached Components");
                for (ComponentKind componentKind : componentKinds)
                {
                    ImGui::BulletText("%s", componentKindName(componentKind));
                }
                ImGui::Separator();
            }

            void drawTransform(const Entity &entity)
            {
                if (auto *transform = entity.tryGetTransform())
                {
                    if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
                    {
                        ImGui::DragFloat3("Translation", &transform->translation.x, 0.05f);
                        ImGui::DragFloat3("Rotation", &transform->rotation.x, 0.01f);
                        ImGui::DragFloat3("Scale", &transform->scale.x, 0.05f, 0.01f, 100.0f);
                    }
                }
            }

            void drawMesh(const Entity &entity)
            {
                if (auto *mesh = entity.tryGetMesh())
                {
                    if (ImGui::CollapsingHeader("Mesh", ImGuiTreeNodeFlags_DefaultOpen))
                    {
                        ImGui::Text("Model Handle: %u", mesh->modelHandle.value);
                        ImGui::ColorEdit3("Color", &mesh->color.x);
                    }
                }
            }

            void drawCamera(const Entity &entity)
            {
                if (auto *camera = entity.tryGetCamera())
                {
                    if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen))
                    {
                        ImGui::TextUnformatted(camera->primary ? "Primary Camera" : "Camera");
                        if (!camera->primary && ImGui::Button("Set As Primary"))
                        {
                            entity.setPrimaryCamera();
                        }
                    }
                }
            }

            void drawPointLight(const Entity &entity)
            {
                if (auto *pointLight = entity.tryGetPointLight())
                {
                    if (ImGui::CollapsingHeader("Point Light", ImGuiTreeNodeFlags_DefaultOpen))
                    {
                        ImGui::ColorEdit3("Light Color", &pointLight->color.x);
                        ImGui::DragFloat("Intensity", &pointLight->intensity, 0.05f, 0.0f, 100.0f);
                        ImGui::DragFloat("Radius", &pointLight->radius, 0.01f, 0.01f, 10.0f);
                    }
                }
            }

            bool open = true;
        };
    }

    EditorPanels::EditorPanels()
    {
        panels.push_back(std::make_unique<FrameStatsPanel>());
        panels.push_back(std::make_unique<EditorViewPanel>());
        panels.push_back(std::make_unique<HierarchyPanel>());
        panels.push_back(std::make_unique<InspectorPanel>());
        panels.push_back(std::make_unique<RuntimeViewPanel>());
    }

    EditorPanels::~EditorPanels() = default;

    void EditorPanels::draw(ImGuiFrameData &frameData)
    {
        drawDockspace();

        for (const auto &panel : panels)
        {
            if (!panel->isOpen())
                continue;

            panel->draw(frameData, boundScene, selectedEntity);
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

        if (ImGui::BeginMenuBar())
        {
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
            if (ImGui::BeginMenu("Primitives"))
            {
                drawPrimitiveMenuItem(PrimitiveType::Cube);
                drawPrimitiveMenuItem(PrimitiveType::Sphere);
                drawPrimitiveMenuItem(PrimitiveType::Plane);
                drawPrimitiveMenuItem(PrimitiveType::Capsule);
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }

        ImGui::End();
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