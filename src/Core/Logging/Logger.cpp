#include "Core/Logging/Logger.hpp"

#include "quill/Frontend.h"
#include "quill/sinks/ConsoleSink.h"

namespace Faye
{
    void ConsoleSink::write_log(quill::MacroMetadata const *log_metadata, uint64_t log_timestamp,
                                std::string_view /*thread_id*/, std::string_view /*thread_name*/,
                                std::string const & /*process_id*/, std::string_view /*logger_name*/,
                                quill::LogLevel log_level, std::string_view /*log_level_description*/,
                                std::string_view /*log_level_short_code*/,
                                std::vector<std::pair<std::string, std::string>> const * /*named_args*/,
                                std::string_view log_message, std::string_view /*log_statement*/)
    {
        std::lock_guard lock(_mutex);
        _queue.emplace_back(LogMessage{log_metadata, log_timestamp, log_level, std::string(log_message)});
    }

    void ConsoleSink::flush_sink() noexcept {}

    bool ConsoleSink::has_pending_messages() const
    {
        std::lock_guard lock(_mutex);
        return !_queue.empty();
    }

    LogMessage ConsoleSink::pop_message()
    {
        std::lock_guard lock(_mutex);
        LogMessage msg = _queue.front();
        _queue.pop_front();
        return msg;
    }

    Logger &Logger::instance()
    {
        static Logger inst;
        return inst;
    }

    quill::Logger *Logger::get()
    {
        return instance()._logger;
    }

    std::shared_ptr<ConsoleSink> Logger::getConsoleSink()
    {
        return instance()._consoleSink;
    }

    Logger::Logger()
    {
        auto terminalSink = quill::Frontend::create_or_get_sink<quill::ConsoleSink>("console_sink");
        auto editorSink = quill::Frontend::create_or_get_sink<ConsoleSink>("editor_sink");

        _consoleSink = std::static_pointer_cast<ConsoleSink>(editorSink);

        _logger = quill::Frontend::create_or_get_logger(
            "main", {std::move(terminalSink), std::move(editorSink)});
    }
}
