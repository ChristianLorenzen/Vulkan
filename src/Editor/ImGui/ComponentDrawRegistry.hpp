#pragma once

#include <unordered_map>

#include <imgui.h>

#include "Core/ECS/ComponentPool.hpp"
#include "Scene/Entities/Entity.hpp"

namespace Faye
{
    // Context handed to every component drawer: the scene facade for the
    // drawn entity, for the rare drawer that needs more than the component
    // data itself (e.g. Camera's "Set As Primary" button).
    struct ComponentDrawContext
    {
        Entity entity;
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
