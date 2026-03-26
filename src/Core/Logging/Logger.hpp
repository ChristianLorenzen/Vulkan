#pragma once

#include "quill/Backend.h"
#include "quill/Frontend.h"
#include "quill/LogMacros.h"
#include "quill/Logger.h"
#include "quill/sinks/ConsoleSink.h"

class Logger {
public:
    static quill::Logger* getInstance() {
        static Logger instance;
        return instance._logger;
    }

    // quill::Logger* getLogger() {
    //     return _logger;
    // }

    const char* loggerName = "console_logger";

private:
    Logger() {
        auto sink = quill::Frontend::create_or_get_sink<quill::ConsoleSink>("console_sink");
        _logger = quill::Frontend::create_or_get_logger(
            loggerName, std::move(sink));
    }

    ~Logger() {
    }

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    quill::Logger* _logger;
};
