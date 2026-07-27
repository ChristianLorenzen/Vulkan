#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui_internal.h"
#include "ImGuiCustomStyle.hpp"

// VS Code Dark+ / GitHub Dark inspired theme.
// ImGui renders in linear colour space without gamma correction, so the float
// values must be darker than their sRGB hex equivalents to look the same.
//
//  Deep bg        #0d0d0d  ->  0.051   window backgrounds, empty dockspace
//  Panel bg       #141414  ->  0.082   child panels
//  Raised surface #1e1e1e  ->  0.118   title bars, menu bar
//  Input frame    #2a2a2a  ->  0.165   text inputs, checkboxes
//  Border         #383838  ->  0.220
//  Accent blue    #0078d4  ->  (0.000, 0.471, 0.831)
//  Text           #cccccc  ->  0.800
//  Dim text       #6e6e6e  ->  0.431

void Faye::Editor::ImGuiIntegration::StyleColorsCustom(ImGuiStyle *dst)
{
    ImGuiStyle *style = dst ? dst : &ImGui::GetStyle();
    ImVec4 *colors = style->Colors;

    const ImVec4 accent = ImVec4(0.000f, 0.471f, 0.831f, 1.00f);
    const ImVec4 accentHover = ImVec4(0.122f, 0.541f, 0.824f, 1.00f);
    const ImVec4 accentActive = ImVec4(0.000f, 0.400f, 0.720f, 1.00f);
    const ImVec4 selection = ImVec4(0.149f, 0.310f, 0.471f, 1.00f);

    // Text
    colors[ImGuiCol_Text] = ImVec4(0.800f, 0.800f, 0.800f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.431f, 0.431f, 0.431f, 1.00f);

    // Backgrounds
    colors[ImGuiCol_WindowBg] = ImVec4(0.051f, 0.051f, 0.051f, 1.00f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.000f, 0.000f, 0.000f, 0.00f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.082f, 0.082f, 0.086f, 0.98f);

    // Borders
    colors[ImGuiCol_Border] = ImVec4(0.220f, 0.220f, 0.220f, 0.65f);
    colors[ImGuiCol_BorderShadow] = ImVec4(0.000f, 0.000f, 0.000f, 0.00f);

    // Frames (inputs, checkboxes, sliders)
    colors[ImGuiCol_FrameBg] = ImVec4(0.165f, 0.165f, 0.165f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.210f, 0.210f, 0.210f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.149f, 0.310f, 0.471f, 1.00f);

    // Title bars
    colors[ImGuiCol_TitleBg] = ImVec4(0.051f, 0.051f, 0.051f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.118f, 0.118f, 0.118f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.051f, 0.051f, 0.051f, 0.80f);

    // Menu bar
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.082f, 0.082f, 0.082f, 1.00f);

    // Scrollbar
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.031f, 0.031f, 0.031f, 1.00f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.200f, 0.200f, 0.200f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.290f, 0.290f, 0.290f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.390f, 0.390f, 0.390f, 1.00f);

    // Interactive widgets
    colors[ImGuiCol_CheckMark] = accent;
    colors[ImGuiCol_SliderGrab] = accentHover;
    colors[ImGuiCol_SliderGrabActive] = accent;

    colors[ImGuiCol_Button] = ImVec4(0.165f, 0.165f, 0.165f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = accentHover;
    colors[ImGuiCol_ButtonActive] = accentActive;

    // Headers (tree nodes, selectables, collapsibles)
    colors[ImGuiCol_Header] = ImVec4(0.149f, 0.310f, 0.471f, 0.50f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.149f, 0.310f, 0.471f, 0.80f);
    colors[ImGuiCol_HeaderActive] = selection;

    // Separators
    colors[ImGuiCol_Separator] = ImVec4(0.220f, 0.220f, 0.220f, 0.50f);
    colors[ImGuiCol_SeparatorHovered] = accentHover;
    colors[ImGuiCol_SeparatorActive] = accent;

    // Resize grip
    colors[ImGuiCol_ResizeGrip] = ImVec4(0.000f, 0.471f, 0.831f, 0.15f);
    colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.000f, 0.471f, 0.831f, 0.55f);
    colors[ImGuiCol_ResizeGripActive] = ImVec4(0.000f, 0.471f, 0.831f, 0.85f);

    // Tabs
    colors[ImGuiCol_Tab] = ImVec4(0.082f, 0.082f, 0.082f, 1.00f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.140f, 0.140f, 0.140f, 1.00f);
    colors[ImGuiCol_TabSelected] = ImVec4(0.051f, 0.051f, 0.051f, 1.00f);
    colors[ImGuiCol_TabSelectedOverline] = accent;
    colors[ImGuiCol_TabDimmed] = ImVec4(0.063f, 0.063f, 0.063f, 1.00f);
    colors[ImGuiCol_TabDimmedSelected] = ImVec4(0.100f, 0.100f, 0.100f, 1.00f);
    colors[ImGuiCol_TabDimmedSelectedOverline] = ImVec4(0.220f, 0.220f, 0.220f, 0.50f);

    // Docking
    colors[ImGuiCol_DockingPreview] = ImVec4(0.000f, 0.471f, 0.831f, 0.50f);
    colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.031f, 0.031f, 0.031f, 1.00f);

    // Plots
    colors[ImGuiCol_PlotLines] = ImVec4(0.350f, 0.700f, 0.350f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.250f, 0.850f, 0.250f, 1.00f);
    colors[ImGuiCol_PlotHistogram] = accent;
    colors[ImGuiCol_PlotHistogramHovered] = accentHover;

    // Tables
    colors[ImGuiCol_TableHeaderBg] = ImVec4(0.118f, 0.118f, 0.118f, 1.00f);
    colors[ImGuiCol_TableBorderStrong] = ImVec4(0.220f, 0.220f, 0.220f, 1.00f);
    colors[ImGuiCol_TableBorderLight] = ImVec4(0.140f, 0.140f, 0.140f, 1.00f);
    colors[ImGuiCol_TableRowBg] = ImVec4(0.000f, 0.000f, 0.000f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.000f, 1.000f, 1.000f, 0.03f);

    // Misc
    colors[ImGuiCol_TextLink] = accent;
    colors[ImGuiCol_TextSelectedBg] = ImVec4(0.149f, 0.310f, 0.471f, 0.65f);
    colors[ImGuiCol_DragDropTarget] = ImVec4(0.000f, 0.471f, 0.831f, 0.90f);
    colors[ImGuiCol_NavCursor] = accent;
    colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.000f, 1.000f, 1.000f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.000f, 0.000f, 0.000f, 0.50f);
    colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.000f, 0.000f, 0.000f, 0.60f);
}

void Faye::Editor::ImGuiIntegration::SetColourThemePabloDark(ImGuiStyle *dst)
{
    ImGuiStyle &style = dst ? *dst : ImGui::GetStyle();
    ImVec4 *colors = style.Colors;

    // Corners
    style.WindowRounding = 8.0f;
    style.ChildRounding = 8.0f;
    style.FrameRounding = 6.0f;
    style.PopupRounding = 6.0f;
    style.ScrollbarRounding = 6.0f;
    style.GrabRounding = 6.0f;
    style.TabRounding = 6.0f;

    // Colors
    colors[ImGuiCol_Text] = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    colors[ImGuiCol_WindowBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.08f, 0.08f, 0.08f, 0.94f);
    colors[ImGuiCol_Border] = ImVec4(0.25f, 0.25f, 0.25f, 0.70f);
    colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);
    colors[ImGuiCol_MenuBarBg] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);
    colors[ImGuiCol_CheckMark] = ImVec4(0.80f, 0.80f, 0.80f, 1.00f);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.70f, 0.70f, 0.70f, 1.00f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.85f, 0.85f, 0.85f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);
    colors[ImGuiCol_Header] = ImVec4(0.25f, 0.25f, 0.25f, 0.55f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.35f, 0.35f, 0.35f, 0.80f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.40f, 0.40f, 0.40f, 1.00f);
    colors[ImGuiCol_Separator] = ImVec4(0.30f, 0.30f, 0.30f, 0.50f);
    colors[ImGuiCol_SeparatorHovered] = ImVec4(0.45f, 0.45f, 0.45f, 0.78f);
    colors[ImGuiCol_SeparatorActive] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    colors[ImGuiCol_ResizeGrip] = ImVec4(0.30f, 0.30f, 0.30f, 0.25f);
    colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.45f, 0.45f, 0.45f, 0.67f);
    colors[ImGuiCol_ResizeGripActive] = ImVec4(0.50f, 0.50f, 0.50f, 0.95f);
    colors[ImGuiCol_Tab] = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.30f, 0.30f, 0.30f, 0.80f);
    colors[ImGuiCol_TabActive] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
    colors[ImGuiCol_TabUnfocused] = ImVec4(0.10f, 0.10f, 0.10f, 0.97f);
    colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
    colors[ImGuiCol_DockingPreview] = ImVec4(0.30f, 0.30f, 0.30f, 0.70f);
    colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
    colors[ImGuiCol_PlotLines] = ImVec4(0.70f, 0.70f, 0.70f, 1.00f);
    colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.90f, 0.50f, 0.50f, 1.00f);
    colors[ImGuiCol_PlotHistogram] = ImVec4(0.80f, 0.65f, 0.00f, 1.00f);
    colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.90f, 0.50f, 0.00f, 1.00f);
    colors[ImGuiCol_TextSelectedBg] = ImVec4(0.50f, 0.50f, 0.50f, 0.35f);
    colors[ImGuiCol_DragDropTarget] = ImVec4(1.00f, 0.00f, 0.00f, 0.90f);
    colors[ImGuiCol_NavHighlight] = ImVec4(0.70f, 0.70f, 0.70f, 1.00f);
    colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
    colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);
}