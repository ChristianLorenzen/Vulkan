#pragma once

#include <cstdint>
#include <functional>

#include <imgui.h>

#include "Core/Handles/MaterialHandle.hpp"
#include "Editor/ImGuiFrameData.hpp"
#include "Renderer/Material/Material.hpp"
#include "engine/Scene/Scene.hpp"
namespace Faye
{
    class MaterialRegistry;
    class MaterialTemplateRegistry;
    class ModelRegistry;
}

namespace Faye::Editor::Widgets
{
    class EditorIconLibrary;
}

namespace Faye::Editor::Panels
{
    // One dockable editor window. EditorPanels owns the list and hands every
    // panel the same per-frame state; a panel ignores the arguments it has no
    // use for.
    class IEditorPanel
    {
    public:
        using TextureThumbnailCallback = std::function<ImTextureID(MaterialHandle, TextureType)>;

        virtual ~IEditorPanel() = default;

        virtual const char *getName() const = 0;
        virtual bool isOpen() const = 0;
        virtual void setOpen(bool open) = 0;
        virtual bool showInViewMenu() const { return true; }
        // Panels that draw file grids (asset explorer, texture picker) opt in;
        // the rest ignore it. EditorPanels owns the one shared library.
        virtual void setIconLibrary(const Widgets::EditorIconLibrary *library) { (void)library; }
        virtual void draw(ImGuiFrameData &frameData,
                          Scene *scene,
                          Entity &selectedEntity,
                          uint32_t &selectedMeshNodeIndex,
                          MaterialRegistry *materialRegistry = nullptr,
                          ModelRegistry *modelRegistry = nullptr,
                          const TextureThumbnailCallback *textureThumbnailCallback = nullptr,
                          MaterialTemplateRegistry *materialTemplateRegistry = nullptr) = 0;

    protected:
        bool open = true;
    };
}
