#pragma once

#include "Core/ECS/Reflection/TypeDescriptor.hpp"
#include "Editor/Utility/ComponentDrawRegistry.hpp"

namespace Faye::Editor::Panels
{
    // Draws every field of a reflected type as inspector rows, using only the
    // TypeDescriptor -- no per-component code. This is the fallback the
    // inspector reaches for when ComponentDrawRegistry has no hand-written
    // drawer for the type, and the eventual replacement for most of them.
    //
    // Returns true if any field was edited this frame. Fires the type's
    // onFieldChanged hook per edited field, so invariants (one primary camera)
    // repair themselves exactly as they do through a hand-written drawer.
    bool drawFields(const Ecs::TypeDescriptor &type, void *object,
                    const Utility::ComponentDrawContext &context);
}
