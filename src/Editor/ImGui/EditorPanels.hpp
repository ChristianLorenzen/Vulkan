#pragma once

#include <functional>
#include <memory>
#include <vector>

#include "Assets/ModelRegistry.hpp"
#include "Renderer/Resources/PrimitiveType.hpp"
#include "Renderer/Frame/ImGuiFrameData.hpp"
#include "Renderer/Material/MaterialRegistry.hpp"
#include "Scene/Scene.hpp"

namespace Faye
{
    class ScriptSystem;

    class IEditorPanel
    {
    public:
        using TextureThumbnailCallback = std::function<ImTextureID(MaterialHandle, TextureType)>;

        virtual ~IEditorPanel() = default;

        virtual const char *getName() const = 0;
        virtual bool isOpen() const = 0;
        virtual void setOpen(bool open) = 0;
        virtual bool showInViewMenu() const { return true; }
        virtual void bindScriptSystem(ScriptSystem *) {}
        virtual void draw(ImGuiFrameData &frameData,
                          Scene *scene,
                          Entity &selectedEntity,
                          MaterialRegistry *materialRegistry = nullptr,
                          ModelRegistry *modelRegistry = nullptr,
                          const TextureThumbnailCallback *textureThumbnailCallback = nullptr) = 0;
    };

    class EditorPanels
    {
    public:
        using PrimitiveCreateCallback = std::function<Entity(PrimitiveType)>;
        using TextureThumbnailCallback = IEditorPanel::TextureThumbnailCallback;

        EditorPanels();
        ~EditorPanels();

        void bindScene(Scene *scene) { boundScene = scene; }
        void setPrimitiveCreateCallback(PrimitiveCreateCallback callback) { primitiveCreateCallback = std::move(callback); }
        void setSelectedEntity(Entity entity) { selectedEntity = entity; }
        Entity getSelectedEntity() const { return selectedEntity; }
        void setMaterialRegistry(MaterialRegistry *registry) { materialRegistry = registry; }
        void setModelRegistry(ModelRegistry *registry) { modelRegistry = registry; }
        void setTextureThumbnailCallback(TextureThumbnailCallback callback) { textureThumbnailCallback = std::move(callback); }
        void setScriptSystem(ScriptSystem *sys) { scriptSystem = sys; }
        void draw(ImGuiFrameData &frameData);

    private:
        void drawDockspace();
        void drawPrimitiveMenuItem(PrimitiveType primitiveType);

        Scene *boundScene = nullptr;
        Entity selectedEntity;
        PrimitiveCreateCallback primitiveCreateCallback;
        std::vector<std::unique_ptr<IEditorPanel>> panels;

        MaterialRegistry *materialRegistry = nullptr;
        ModelRegistry *modelRegistry = nullptr;
        TextureThumbnailCallback textureThumbnailCallback;
        ScriptSystem *scriptSystem = nullptr;
    };
}