#include "Editor/Panels/AssetExplorer/AssetExplorerPanel.hpp"

#include "Core/HotReload/HotReloadSystem.hpp"

#include "imgui.h"

namespace Faye::Editor::Panels
{
    void AssetExplorerPanel::FileChangeCallback(const HotReloadEvent &event)
    {
        // Content changes don't move files around, so only creates/deletes/
        // renames need the directory listing rebuilt.
        if (event.type != HotReloadEventType::Modified)
        {
            browser.markDirty();
        }
    }

    void AssetExplorerPanel::setInitialFileWatch(WatchState watchState)
    {
        this->watchState = watchState;
        browser.setRoot(watchState.spec.rootPath);
    }

    void AssetExplorerPanel::draw(ImGuiFrameData &frameData,
                                  Scene *scene,
                                  Entity &selectedEntity,
                                  uint32_t &selectedMeshNodeIndex,
                                  MaterialRegistry *materialRegistry,
                                  ModelRegistry *modelRegistry,
                                  const TextureThumbnailCallback *textureThumbnailCallback,
                                  MaterialTemplateRegistry *materialTemplateRegistry)
    {
        (void)frameData;
        (void)scene;
        (void)selectedEntity;
        (void)selectedMeshNodeIndex;
        (void)materialRegistry;
        (void)modelRegistry;
        (void)textureThumbnailCallback;
        (void)materialTemplateRegistry;

        if (!open)
            return;

        if (ImGui::Begin(getName(), &open))
        {
            if (icons != nullptr)
            {
                browser.drawBreadcrumb();
                ImGui::Separator();

                // Deferred: the callback imports a model and mutates the scene,
                // which is not safe to do while the popup owns the ImGui frame.
                browser.draw(*icons, [this](const std::filesystem::path &path) {
                    if (ImGui::MenuItem("Create Entity"))
                        pendingCreationPath = path;
                });

                if (!pendingCreationPath.empty())
                {
                    if (entityCreateCallback)
                        entityCreateCallback(pendingCreationPath);
                    pendingCreationPath.clear();
                }
            }
        }

        ImGui::End();
    }
}
