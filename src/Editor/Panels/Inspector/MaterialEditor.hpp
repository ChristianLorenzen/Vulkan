#pragma once

#include <filesystem>
#include <string_view>

#include "Core/Handles/MaterialHandle.hpp"
#include "Editor/Utility/ComponentDrawRegistry.hpp"
#include "Renderer/Material/Material.hpp"

namespace Faye
{
    class MaterialRegistry;
    class MaterialTemplateRegistry;
}

namespace Faye::Editor::Panels
{
    // Material editing UI shared by the inspector's Model Materials section and
    // by the Mesh Renderer component drawer: both edit a material in place,
    // and the drawers are plain functions in the draw table rather than
    // InspectorPanel members.

    // Name/shader header, the property grid for the active template, and the
    // texture slots. `usageSummary` is the (optional) list of submeshes using
    // this material, shown in the handle tooltip.
    void drawMaterialProperties(MaterialHandle handle,
                                Material &material,
                                std::string_view usageSummary,
                                const Utility::ComponentDrawContext::TextureThumbnailFn *thumbnails,
                                MaterialTemplateRegistry *templateRegistry,
                                Widgets::TexturePickerPopup *picker);

    // drawMaterialProperties wrapped in a labelled tree node, for listing
    // several materials in a row.
    void drawMaterialEntry(const char *label,
                           MaterialHandle handle,
                           MaterialRegistry *materialRegistry,
                           std::string_view usageSummary,
                           const Utility::ComponentDrawContext::TextureThumbnailFn *thumbnails,
                           MaterialTemplateRegistry *templateRegistry,
                           Widgets::TexturePickerPopup *picker);

    // Replaces the material's texture of this type, or appends one if the
    // slot was empty. Returns false (and leaves the material untouched) if
    // the file cannot be decoded.
    bool assignTexture(Material &material, TextureType type, const std::filesystem::path &path);
}
