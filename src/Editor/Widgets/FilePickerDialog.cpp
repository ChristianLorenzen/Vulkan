#include "Editor/Widgets/FilePickerDialog.hpp"

#include <algorithm>

namespace Faye::Editor::Widgets
{
    void FilePickerDialog::open(Mode mode, std::vector<std::string> extensions, std::filesystem::path start)
    {
        pickerMode = mode;
        extensionFilter = std::move(extensions);

        std::error_code ec;
        if (!std::filesystem::is_directory(start, ec))
        {
            start = start.parent_path();
            if (!std::filesystem::is_directory(start, ec))
                start = Paths::projects();
        }

        browser.setRoot(start);
        browser.setExtensionFilter(extensionFilter);
        accepted.clear();
        fileNameBuffer[0] = '\0';
        openRequested = true;
        opened = false;
    }

    bool FilePickerDialog::draw(const EditorIconLibrary &icons)
    {
        if (openRequested)
        {
            ImGui::OpenPopup(kPopupName);
            openRequested = false;
        }

        ImGui::SetNextWindowSize(ImVec2(680.0f, 480.0f), ImGuiCond_Appearing);
        if (!ImGui::BeginPopupModal(kPopupName, nullptr, ImGuiWindowFlags_NoSavedSettings))
        {
            return false;
        }
        opened = true;

        browser.drawBreadcrumb();
        ImGui::Separator();

        bool acceptedThisFrame = false;
        if (ImGui::BeginChild("##pickerGrid", ImVec2(0.0f, -ImGui::GetFrameHeightWithSpacing())))
        {
            const FileBrowserView::Result result = browser.draw(icons);

            if (pickerMode == Mode::Open)
            {
                if (result.fileActivated)
                {
                    accepted = result.activatedPath;
                    acceptedThisFrame = true;
                    ImGui::CloseCurrentPopup();
                }
            }
            else
            {
                // Save mode: double-clicking a matching file pre-fills the
                // name (so it can be tweaked) instead of accepting outright.
                if (result.fileActivated)
                {
                    const std::string name = result.activatedPath.filename().string();
                    const size_t count = std::min(name.size(), fileNameBuffer.size() - 1);
                    std::copy(name.begin(), name.begin() + count, fileNameBuffer.begin());
                    fileNameBuffer[count] = '\0';
                }
            }
        }
        ImGui::EndChild();

        if (pickerMode == Mode::Save)
        {
            const std::string dirText = "Save into: " + browser.getCurrentPath().string();
            ImGui::TextUnformatted(dirText.c_str());
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 140.0f);
            ImGui::InputText("##fileName", fileNameBuffer.data(), static_cast<int>(fileNameBuffer.size()));
            ImGui::SameLine();
            const bool saveClicked = ImGui::Button("Save") || ImGui::IsKeyPressed(ImGuiKey_Enter);
            ImGui::SameLine();
            const bool cancelClicked = ImGui::Button("Cancel");

            if (saveClicked && fileNameBuffer[0] != '\0')
            {
                accepted = browser.getCurrentPath();
                acceptedThisFrame = true;
                ImGui::CloseCurrentPopup();
            }
            else if (cancelClicked)
            {
                ImGui::CloseCurrentPopup();
            }
        }
        else
        {
            ImGui::TextDisabled("Double-click a file to open it.");
            ImGui::SameLine();
            if (ImGui::Button("Cancel"))
            {
                ImGui::CloseCurrentPopup();
            }
        }

        ImGui::EndPopup();
        return acceptedThisFrame;
    }

    std::filesystem::path FilePickerDialog::savePath() const
    {
        const std::string name(fileNameBuffer.data());
        if (name.empty())
            return {};

        std::filesystem::path result = accepted / name;
        if (!result.has_extension() && !extensionFilter.empty())
        {
            result += extensionFilter.front();   // e.g. ".faye"
        }
        return result;
    }
}
