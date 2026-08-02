#pragma once

#include <memory>
#include <vector>

#include "Core/Logging/Logger.hpp"
#include "Editor/Panels/IEditorPanel.hpp"

namespace Faye::Editor::Panels
{
    // Drains the logger's console sink into a scrolling table, colour-coded by
    // log level.
    class ConsolePanel final : public IEditorPanel
    {
    public:
        explicit ConsolePanel(std::shared_ptr<ConsoleSink> consoleSink);

        const char *getName() const override { return "Console"; }
        bool isOpen() const override { return open; }
        void setOpen(bool isOpen) override { open = isOpen; }

        void draw(ImGuiFrameData &frameData,
                  Scene *scene,
                  Entity &selectedEntity,
                  uint32_t &selectedMeshNodeIndex,
                  MaterialRegistry *materialRegistry,
                  ModelRegistry *modelRegistry,
                  const TextureThumbnailCallback *textureThumbnailCallback,
                  MaterialTemplateRegistry *materialTemplateRegistry) override;

    private:
        void popMessages();

        std::shared_ptr<ConsoleSink> sink;
        std::vector<LogMessage> messages;
        bool scroll = false;
    };
}
