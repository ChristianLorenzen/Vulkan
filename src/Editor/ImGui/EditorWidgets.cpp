#include "Editor/ImGui/EditorWidgets.hpp"

#include <algorithm>
#include <cfloat>
#include <cstring>

namespace Faye::EditorUI
{
    namespace
    {
        // Matches the "raised surface" step of the editor theme (see
        // ImGuiCustomStyle.cpp): the card header sits one shade above the panel
        // background so it reads as a title bar rather than a selection.
        constexpr ImVec4 kCardHeader{0.118f, 0.118f, 0.118f, 1.0f};
        constexpr ImVec4 kCardHeaderHovered{0.160f, 0.160f, 0.160f, 1.0f};
        constexpr ImVec4 kCardHeaderActive{0.196f, 0.196f, 0.196f, 1.0f};
        // One step below the card header, so a section inside a card does not
        // compete with the card's own title bar.
        constexpr ImVec4 kSubSectionHeader{0.086f, 0.086f, 0.086f, 1.0f};
        constexpr ImVec4 kSubSectionHovered{0.129f, 0.129f, 0.129f, 1.0f};
        constexpr ImVec4 kSubSectionActive{0.165f, 0.165f, 0.165f, 1.0f};
        constexpr ImVec4 kRemoveHovered{0.600f, 0.180f, 0.180f, 1.0f};
        constexpr ImVec4 kRemoveActive{0.480f, 0.140f, 0.140f, 1.0f};
        constexpr ImVec2 kHeaderPadding{8.0f, 6.0f};
        constexpr ImVec2 kCardPadding{8.0f, 6.0f};

        // Label column takes a fixed share of the panel so every row in every
        // component lines up, even across separate tables.
        constexpr float kLabelColumnWeight = 0.40f;
        constexpr float kValueColumnWeight = 0.60f;
    }

    bool beginProperties(const char *id)
    {
        constexpr ImGuiTableFlags flags = ImGuiTableFlags_SizingStretchProp |
                                          ImGuiTableFlags_NoBordersInBody |
                                          ImGuiTableFlags_PadOuterX;

        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(4.0f, 3.0f));
        if (ImGui::BeginTable(id, 2, flags))
        {
            ImGui::TableSetupColumn("##label", ImGuiTableColumnFlags_WidthStretch, kLabelColumnWeight);
            ImGui::TableSetupColumn("##value", ImGuiTableColumnFlags_WidthStretch, kValueColumnWeight);
            return true;
        }

        ImGui::PopStyleVar();
        return false;
    }

    void endProperties()
    {
        ImGui::EndTable();
        ImGui::PopStyleVar();
    }

    void propertyLabel(const char *label, const char *tooltip)
    {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(label);

        if (tooltip != nullptr && tooltip[0] != '\0')
        {
            ImGui::SameLine(0.0f, 4.0f);
            ImGui::TextDisabled("(?)");
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
            {
                ImGui::SetTooltip("%s", tooltip);
            }
        }

        ImGui::TableSetColumnIndex(1);
        // -FLT_MIN = "fill the cell", so the widget always reaches the right
        // edge no matter how the panel is resized.
        ImGui::SetNextItemWidth(-FLT_MIN);
    }

    void propertyText(const char *label, const char *value, const char *tooltip)
    {
        propertyLabel(label, tooltip);
        ImGui::AlignTextToFramePadding();
        // The table cell clips overlong values; hovering recovers the full text.
        ImGui::TextUnformatted(value);
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        {
            ImGui::SetTooltip("%s", value);
        }
    }

    ComponentCard beginComponentCard(const char *title, bool removable, const char *tooltip)
    {
        ComponentCard card;

        ImGui::PushID(title);

        // SameLine() offsets are measured from the window origin, not from the
        // content region, so the line's start X has to be added back or the
        // button lands one WindowPadding short of the right edge.
        const float lineStartX = ImGui::GetCursorPosX();
        const float availableWidth = ImGui::GetContentRegionAvail().x;
        const float headerHeight = ImGui::GetFontSize() + kHeaderPadding.y * 2.0f;

        ImGui::PushStyleColor(ImGuiCol_Header, kCardHeader);
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, kCardHeaderHovered);
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, kCardHeaderActive);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, kHeaderPadding);

        // AllowOverlap is what makes the remove button clickable: the header
        // spans the full width, so without it the header owns the hovered id
        // for those pixels and the button never registers a click.
        ImGui::SetNextItemAllowOverlap();
        card.open = ImGui::CollapsingHeader(
            title, ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap);

        ImGui::PopStyleVar();
        ImGui::PopStyleColor(3);

        if (tooltip != nullptr && tooltip[0] != '\0' && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        {
            ImGui::SetTooltip("%s", tooltip);
        }

        if (removable)
        {
            ImGui::SameLine(lineStartX + availableWidth - headerHeight);
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, kRemoveHovered);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, kRemoveActive);
            card.removeRequested = ImGui::Button("x", ImVec2(headerHeight, headerHeight));
            ImGui::PopStyleColor(3);
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
            {
                ImGui::SetTooltip("Remove component");
            }
        }

        if (card.open)
        {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, kCardPadding);
            ImGui::BeginChild("##cardBody",
                              ImVec2(0.0f, 0.0f),
                              ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders);
            ImGui::PopStyleVar();
        }

        return card;
    }

    bool subSection(const char *label, bool defaultOpen)
    {
        ImGui::PushStyleColor(ImGuiCol_Header, kSubSectionHeader);
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, kSubSectionHovered);
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, kSubSectionActive);

        const ImGuiTreeNodeFlags flags = defaultOpen ? ImGuiTreeNodeFlags_DefaultOpen : 0;
        const bool open = ImGui::CollapsingHeader(label, flags);

        ImGui::PopStyleColor(3);
        return open;
    }

    void endComponentCard(const ComponentCard &card)
    {
        if (card.open)
        {
            ImGui::EndChild();
        }

        ImGui::PopID();
        ImGui::Spacing();
    }

    void copyNameToBuffer(std::string_view value, std::array<char, 128> &buffer)
    {
        buffer.fill('\0');

        const size_t copyLength = std::min(buffer.size() - 1, value.size());
        std::memcpy(buffer.data(), value.data(), copyLength);
    }
}
