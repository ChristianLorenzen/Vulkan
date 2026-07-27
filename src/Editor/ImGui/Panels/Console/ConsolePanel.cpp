#include "Editor/ImGui/Panels/Console/ConsolePanel.hpp"

#include "imgui.h"

namespace Faye
{
    namespace
    {
        // Only warnings and above get a tinted row; everything below reads as
        // ordinary output.
        constexpr unsigned int kRowTintMask =
            (1u << (static_cast<int>(quill::LogLevel::Critical) + 1)) - (1u << static_cast<int>(quill::LogLevel::Warning));

        constexpr ImU32 kLogLevelColors[11] = {
            IM_COL32(128, 128, 128, 255), // TraceL3 - Gray
            IM_COL32(128, 128, 128, 255), // TraceL2 - Gray
            IM_COL32(128, 128, 128, 255), // TraceL1 - Gray
            IM_COL32(128, 128, 128, 255), // Debug - Gray
            IM_COL32(128, 128, 128, 255), // Info - Gray
            IM_COL32(0, 255, 255, 51),    // Notice - Cyan (20% opacity)
            IM_COL32(255, 255, 0, 51),    // Warning - Yellow (20% opacity)
            IM_COL32(255, 128, 0, 51),    // Error  - Orange (20% opacity)
            IM_COL32(255, 0, 0, 51),      // Critical - Red (20% opacity)
            IM_COL32(128, 128, 128, 255), // Trace - Gray (catch-all for all trace levels)
            IM_COL32(128, 128, 128, 255)  // None - Gray (should not be used)
        };

        const char *logLevelToString(quill::LogLevel level)
        {
            switch (level)
            {
            case quill::LogLevel::Debug:
                return "Debug";
            case quill::LogLevel::Info:
                return "Info";
            case quill::LogLevel::Notice:
                return "Notice";
            case quill::LogLevel::Warning:
                return "Warning";
            case quill::LogLevel::Error:
                return "Error";
            case quill::LogLevel::Critical:
                return "Critical";
            case quill::LogLevel::None:
                return "None";
            default:
                return "Unknown";
            }
        }
    }

    ConsolePanel::ConsolePanel(std::shared_ptr<ConsoleSink> consoleSink) : sink(std::move(consoleSink)) {}

    void ConsolePanel::draw(ImGuiFrameData &frameData,
                            Scene *scene,
                            Entity &selectedEntity,
                            uint32_t &selectedMeshNodeIndex,
                            MaterialRegistry *materialRegistry,
                            ModelRegistry *modelRegistry,
                            const TextureThumbnailCallback *textureThumbnailCallback,
                            MaterialTemplateRegistry *materialTemplateRegistry)
    {
        (void)frameData;
        (void)scene;
        (void)selectedEntity;
        (void)selectedMeshNodeIndex;
        (void)materialRegistry;
        (void)modelRegistry;
        (void)textureThumbnailCallback;
        (void)materialTemplateRegistry;

        if (!open)
        {
            // If the console is closed, just flush pending messages so we don't get a buildup.
            // TODO: Revisit if this is what we want. Maybe console sink should use a ring buffer.
            sink->flush_sink();
            return;
        }

        popMessages();

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(15.0f, 10.0f));

        if (ImGui::Begin(getName(), &open))
        {
            if (ImGui::BeginTable("##console", 3, ImGuiTableFlags_BordersInnerV))
            {
                ImGui::TableSetupColumn("Level", ImGuiTableColumnFlags_WidthFixed, 150.0f);
                ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthFixed, 80.0f);
                ImGui::TableSetupColumn("Message", ImGuiTableColumnFlags_WidthStretch);

                for (const LogMessage &log : messages)
                {
                    ImGui::TableNextRow();
                    if ((1 << static_cast<int>(log.level)) & kRowTintMask)
                        ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, kLogLevelColors[static_cast<int>(log.level)]);
                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("%lu", log.timestamp);
                    ImGui::TableSetColumnIndex(1);
                    ImGui::Text("%s", logLevelToString(log.level));
                    ImGui::TableSetColumnIndex(2);
                    ImGui::Text("%s", log.message.c_str());
                }
                ImGui::EndTable();
            }
        }

        if (scroll)
        {
            ImGui::SetScrollHereY(1.0f); // Auto-scroll to bottom
            scroll = false;
        }

        ImGui::End();

        ImGui::PopStyleVar();
    }

    void ConsolePanel::popMessages()
    {
        scroll = scroll || sink->has_pending_messages();

        while (sink->has_pending_messages())
        {
            messages.push_back(sink->pop_message());
        }
    }
}
