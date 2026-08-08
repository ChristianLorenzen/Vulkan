#include <stdio.h>
#include <stdlib.h>
#include <exception>
#include <iostream>
#include <fstream>

#include "Editor/Editor.hpp"

#include "quill/Backend.h"
#include "quill/Frontend.h"
#include "quill/LogMacros.h"
#include "quill/Logger.h"
#include "quill/sinks/ConsoleSink.h"

using namespace Faye;

int main(int argc, char **argv)
{
    quill::Backend::start();

    Faye::Editor::Application editor;

    // Startup scene: --scene <path> wins over the FAYE_SCENE env var. When
    // neither is given, the default scene (SceneBuilder::populate) is used.
    std::string scenePath;
    for (int i = 1; i < argc - 1; ++i)
    {
        if (std::string(argv[i]) == "--scene")
        {
            scenePath = argv[i + 1];
            break;
        }
    }
    if (scenePath.empty())
    {
        if (const char *envScene = std::getenv("FAYE_SCENE"))
            scenePath = envScene;
    }
    if (!scenePath.empty())
        editor.setStartupScenePath(scenePath);

    try
    {
        editor.run();
    }
    catch (const std::exception &e)
    {
        LOG_ERROR(Logger::get(), "Error in Engine.run(): {}", e.what());
        return 1;
    }

    return 0;
}