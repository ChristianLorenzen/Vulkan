#pragma once

#include <functional>
#include <unordered_map>

#include <imgui.h>

#include "Core/ECS/ComponentPool.hpp"
#include "Core/Handles/MaterialHandle.hpp"
#include "Core/Handles/TextureType.hpp"
#include "Editor/Widgets/AssetBrowser.hpp"
#include "engine/Scene/Entities/Entity.hpp"
namespace Faye
{
    class MaterialRegistry;
    class MaterialTemplateRegistry;
    class ModelRegistry;
}

namespace Faye::Ecs
{
    class World;
}

namespace Faye::Editor::Utility
{
    // Context handed to every component drawer: the scene facade for the
    // drawn entity, for the rare drawer that needs more than the component
    // data itself (e.g. Camera's "Set As Primary" button), plus the asset
    // registries a drawer needs to resolve handles into something a human can
    // read and edit (Mesh Renderer's model name and material picker).
    struct ComponentDrawContext
    {
        using TextureThumbnailFn = std::function<ImTextureID(MaterialHandle, TextureType)>;

        Entity entity;
        // The reflected drawer fires TypeDescriptor::onFieldChanged after an
        // edit, and that hook takes a World. Entity keeps its Scene private and
        // exposes only handle(), so the World cannot be recovered from `entity`
        // -- the inspector, which has one in scope, passes it explicitly.
        Ecs::World *world = nullptr;
        MaterialRegistry *materials = nullptr;
        ModelRegistry *models = nullptr;
        const TextureThumbnailFn *thumbnails = nullptr;
        MaterialTemplateRegistry *materialTemplates = nullptr;
        // Owned by the inspector; a drawer only requests that it open.
        Widgets::TexturePickerPopup *texturePicker = nullptr;
    };

    // Editor half of the split reflection design: core keeps the type table
    // (Ecs::ComponentTypeRegistry — name/add/remove/tryGetRaw, no ImGui),
    // this maps the same ComponentId to an ImGui draw function. Joined by id
    // in the inspector loop, so core never includes editor headers.
    class ComponentDrawRegistry
    {
    public:
        using DrawFn = void (*)(const ComponentDrawContext &, void *component);

        // Usage: registerDrawer<TransformComponent, &drawTransform>(). The
        // wrapper lambda performs the static_cast back to T* — legal because
        // the table entry for this id is only ever handed pointers that
        // really are T*. (reinterpret_casting the function pointer itself
        // would be UB; the lambda keeps the cast inside the call.)
        template <class T, void (*DrawTyped)(const ComponentDrawContext &, T &)>
        void registerDrawer()
        {
            drawTable[Ecs::componentId<T>()] =
                [](const ComponentDrawContext &context, void *component)
            {
                DrawTyped(context, *static_cast<T *>(component));
            };
        }

        // Whether a hand-written drawer exists. The inspector asks first so it
        // can fall back to the reflected drawer, which lives in Panels and must
        // not be called from here (Utility does not depend on Panels).
        bool has(Ecs::ComponentId id) const { return drawTable.contains(id); }

        void draw(Ecs::ComponentId id, const ComponentDrawContext &context, void *component) const
        {
            const auto it = drawTable.find(id);
            if (it != drawTable.end())
                it->second(context, component);
            else
                ImGui::TextDisabled("(no editor for this component)");
        }

    private:
        std::unordered_map<Ecs::ComponentId, DrawFn> drawTable;
    };
}
