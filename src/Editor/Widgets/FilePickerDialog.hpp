#pragma once

#include <array>
#include <filesystem>
#include <string>
#include <vector>

#include <imgui.h>

#include "Editor/Widgets/AssetBrowser.hpp"
#include "Core/Path/Paths.hpp"

namespace Faye::Editor::Widgets
{
    // Reusable modal file picker built on FileBrowserView (the same grid the
    // Asset Explorer and texture picker share). One dialog serves any
    // specialized picker: call open(mode, extensions, start) and draw() until
    // it returns true, then read the result. No per-type panel is needed.
    //
    //   Mode::Open — browse + double-click a file (or click Open).
    //   Mode::Save — browse a destination folder + type a file name; the
    //                primary extension is appended when the name has none.
    class FilePickerDialog
    {
    public:
        enum class Mode
        {
            Open,
            Save,
        };

        // Opens the modal (rendered on the next draw()). `extensions` are
        // normalized like FileBrowserView (".faye"); empty shows everything.
        // `start` should be a directory; it falls back to its parent and then
        // Paths::projects() when it does not exist.
        void open(Mode mode, std::vector<std::string> extensions, std::filesystem::path start = Paths::projects());

        // Returns true exactly once, on the frame a path is accepted. Must be
        // called from the same ImGui window that opened the modal.
        bool draw(const EditorIconLibrary &icons);

        Mode mode() const { return pickerMode; }
        // Open mode: the accepted file. Save mode: the chosen directory.
        const std::filesystem::path &path() const { return accepted; }
        // Save mode: directory / typed file name (primary extension appended
        // when missing). Empty when no name was typed.
        std::filesystem::path savePath() const;

    private:
        static constexpr const char *kPopupName = "File Picker";

        FileBrowserView browser;
        Mode pickerMode = Mode::Open;
        std::vector<std::string> extensionFilter;   // copy for savePath() extension appending
        bool openRequested = false;
        bool opened = false;
        std::filesystem::path accepted;
        std::array<char, 256> fileNameBuffer{};
    };
}
