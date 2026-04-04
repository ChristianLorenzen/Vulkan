#pragma once

#include <functional>
#include <memory>
#include <vector>

#include "Renderer/Resources/PrimitiveType.hpp"
#include "Renderer/Frame/ImGuiFrameData.hpp"
#include "Scene/Scene.hpp"

namespace Faye
{
    class IEditorPanel
    {
    public:
        virtual ~IEditorPanel() = default;

        virtual const char *getName() const = 0;
        virtual bool isOpen() const = 0;
        virtual void setOpen(bool open) = 0;
        virtual bool showInViewMenu() const { return true; }
        virtual void draw(ImGuiFrameData &frameData, Scene *scene, Entity &selectedEntity) = 0;
    };

    class EditorPanels
    {
    public:
        using PrimitiveCreateCallback = std::function<Entity(PrimitiveType)>;

        EditorPanels();
        ~EditorPanels();

        void bindScene(Scene *scene) { boundScene = scene; }
        void setPrimitiveCreateCallback(PrimitiveCreateCallback callback) { primitiveCreateCallback = std::move(callback); }
        void setSelectedEntity(Entity entity) { selectedEntity = entity; }
        Entity getSelectedEntity() const { return selectedEntity; }
        void draw(ImGuiFrameData &frameData);

    private:
        void drawDockspace();
        void drawPrimitiveMenuItem(PrimitiveType primitiveType);

        Scene *boundScene = nullptr;
        Entity selectedEntity;
        PrimitiveCreateCallback primitiveCreateCallback;
        std::vector<std::unique_ptr<IEditorPanel>> panels;
    };
}