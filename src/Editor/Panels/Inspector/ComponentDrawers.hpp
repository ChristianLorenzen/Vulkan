#pragma once

#include "Editor/Utility/ComponentDrawRegistry.hpp"

namespace Faye::Editor::Panels
{
    // The inspector's draw table: one entry per component type that has editor
    // UI. Registered in one place so a newly registered component type only
    // needs its drawer written and added here.
    //
    // Drawers themselves are internal to ComponentDrawers.cpp — they are only
    // ever reached through the table, keyed by ComponentId.
    Utility::ComponentDrawRegistry makeEditorDrawRegistry();
}
