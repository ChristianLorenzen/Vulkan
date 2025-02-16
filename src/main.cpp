#include <stdio.h>
#include <stdlib.h>
#include <exception>
#include <iostream>
#include <fstream>

#include "Engine/Engine.hpp"

#include "quill/Backend.h"
#include "quill/Frontend.h"
#include "quill/LogMacros.h"
#include "quill/Logger.h"
#include "quill/sinks/ConsoleSink.h"

using namespace Faye;


int main(int argc, char** argv) {
    quill::Backend::start();
    
    quill::Logger* logger = quill::Frontend::create_or_get_logger(
    "root", quill::Frontend::create_or_get_sink<quill::ConsoleSink>("sink_id_1"));

    Engine app;

    try {
        app.run();
    } catch (const std::exception& e) {
        LOG_ERROR(logger, "Error in Engine.run(): {}", e.what());
        return 1;
    }
    
    return 0;
}