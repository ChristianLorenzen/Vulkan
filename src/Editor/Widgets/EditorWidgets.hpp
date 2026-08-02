#pragma once

#include <array>
#include <string_view>

#include <imgui.h>

// Shared inspector chrome. The editor previously drew every component with
// bare ImGui calls, which left labels at ImGui's default 65% item width and
// gave no visual boundary between one component and the next. These helpers
// impose two conventions:
//
//   * every component lives in a "card" - a tinted header bar plus a bordered
//     body - so the boundary between components is obvious;
//   * every editable field is a row in a two-column table, so labels line up
//     down the panel instead of tracking each widget's width.
namespace Faye::Editor::Widgets
{
    // ---- Property rows ----------------------------------------------------
    // Usage:
    //   if (Widgets::beginProperties("transform"))
    //   {
    //       Widgets::propertyLabel("Position");
    //       ImGui::DragFloat3("##position", &t.translation.x, 0.05f);
    //       Widgets::endProperties();
    //   }
    // Widget labels must be "##hidden" - the visible label is the row label.
    bool beginProperties(const char *id);
    void endProperties();

    // Opens a row: draws the label in column 0 and leaves the cursor in
    // column 1 with the item width already stretched to fill it. A non-empty
    // tooltip adds a hoverable "(?)" beside the label.
    void propertyLabel(const char *label, const char *tooltip = nullptr);

    // A read-only row. Long values are truncated with an ellipsis and the full
    // text is available on hover, which is how paths and handles stay in the
    // inspector without dominating it.
    void propertyText(const char *label, const char *value, const char *tooltip = nullptr);

    // ---- Component cards --------------------------------------------------
    struct ComponentCard
    {
        bool open = false;
        bool removeRequested = false;
    };

    // Draws the header bar (and the remove button when removable). Always pair
    // with endComponentCard, including when `open` comes back false.
    ComponentCard beginComponentCard(const char *title, bool removable, const char *tooltip = nullptr);
    void endComponentCard(const ComponentCard &card);

    // A collapsible group *inside* a card, tinted one step below the card
    // header so the nesting reads. Returns whether the body should be drawn;
    // there is nothing to close. `defaultOpen` only applies the first time the
    // section is seen - after that ImGui remembers what the user chose.
    //
    // Labels that embed changing text (counts, names) must carry a stable id
    // after "###", or toggling state resets whenever the text changes:
    //   subSection("Textures (2/5)###textures", false)
    bool subSection(const char *label, bool defaultOpen = true);

    // ---- Text buffers -----------------------------------------------------
    // Seeds a fixed InputText buffer from a name that is stored as a
    // std::string elsewhere (entity names, material names). Truncates rather
    // than overflowing, and always leaves the buffer nul-terminated.
    void copyNameToBuffer(std::string_view value, std::array<char, 128> &buffer);
}
