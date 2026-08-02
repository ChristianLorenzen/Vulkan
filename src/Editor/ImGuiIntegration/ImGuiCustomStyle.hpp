#pragma once
#include "imgui.h"

// Editor themes. These follow the shape of ImGui's own StyleColors* functions
// (nullptr means "the current style") but live in the editor's namespace, not
// ImGui's, so they are not exported as part of the library.
namespace Faye::Editor::ImGuiIntegration
{
    void StyleColorsCustom(ImGuiStyle *dst = nullptr);
    void SetColourThemePabloDark(ImGuiStyle *dst = nullptr);
}
