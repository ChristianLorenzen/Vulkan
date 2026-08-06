#include "Editor/Utility/FieldDrawRegistry.hpp"

#include <algorithm>

namespace Faye::Editor::Utility
{
    // Opaque is last in the enum, so its index + 1 is the count. If an
    // enumerator is ever appended after Opaque this fires and the table is
    // resized rather than silently truncating the new layout to "no drawer".
    static_assert(static_cast<size_t>(Ecs::Layout::Opaque) + 1 == 20,
                  "Ecs::Layout changed: FieldDrawRegistry::kLayoutCount derives from "
                  "Opaque being the final enumerator");

    void FieldDrawRegistry::registerSemantic(Ecs::TypeId id, FieldDrawFn draw)
    {
        // Id 0 means "no semantic" -- registering it would make every plain
        // struct field resolve to whatever was registered last.
        if (id == 0 || draw == nullptr)
            return;
        semantics[id] = draw;
    }

    void FieldDrawRegistry::registerSemanticExpansion(Ecs::TypeId id, FieldDrawFn draw)
    {
        if (id == 0 || draw == nullptr)
            return;
        expansions[id] = draw;
    }

    void FieldDrawRegistry::registerLayout(Ecs::Layout layout, FieldDrawFn draw)
    {
        const auto index = static_cast<size_t>(layout);
        if (index >= layouts.size() || draw == nullptr)
            return;
        layouts[index] = draw;
    }

    FieldDrawFn FieldDrawRegistry::resolve(const Ecs::FieldDescriptor &field) const
    {
        if (field.typeId != 0)
        {
            if (const auto it = semantics.find(field.typeId); it != semantics.end())
                return it->second;
            // Fall through deliberately: an unregistered semantic still has a
            // usable byte layout. faye.u32 has no semantic drawer and does not
            // need one -- Layout::U32 draws it correctly.
        }

        const auto index = static_cast<size_t>(field.layout);
        return index < layouts.size() ? layouts[index] : nullptr;
    }

    FieldDrawFn FieldDrawRegistry::resolveExpansion(const Ecs::FieldDescriptor &field) const
    {
        if (field.typeId == 0)
            return nullptr;
        const auto it = expansions.find(field.typeId);
        return it != expansions.end() ? it->second : nullptr;
    }

}
