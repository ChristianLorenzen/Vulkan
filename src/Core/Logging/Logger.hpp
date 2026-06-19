#pragma once

#include "quill/LogMacros.h"
#include "quill/Logger.h"
#include "quill/sinks/Sink.h"

#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace Faye
{
    struct LogMessage
    {
        quill::MacroMetadata const *metadata;
        uint64_t timestamp;
        quill::LogLevel level;
        std::string message;
    };

    class ConsoleSink : public quill::Sink
    {
    public:
        ConsoleSink() = default;
        ~ConsoleSink() override = default;

        void write_log(quill::MacroMetadata const *log_metadata, uint64_t log_timestamp,
                       std::string_view thread_id, std::string_view thread_name,
                       std::string const &process_id, std::string_view logger_name,
                       quill::LogLevel log_level, std::string_view log_level_description,
                       std::string_view log_level_short_code,
                       std::vector<std::pair<std::string, std::string>> const *named_args,
                       std::string_view log_message, std::string_view log_statement) override;

        void flush_sink() noexcept override;

        bool has_pending_messages() const;
        LogMessage pop_message();

    private:
        mutable std::mutex _mutex;
        std::deque<LogMessage> _queue;
    };

    class Logger
    {
    public:
        static quill::Logger *get();
        static std::shared_ptr<ConsoleSink> getConsoleSink();

    private:
        Logger();
        Logger(const Logger &) = delete;
        Logger &operator=(const Logger &) = delete;

        static Logger &instance();

        quill::Logger *_logger = nullptr;
        std::shared_ptr<ConsoleSink> _consoleSink;
    };
}
