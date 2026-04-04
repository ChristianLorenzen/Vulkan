#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

#include "Core/Logging/Logger.hpp"

namespace Faye
{
    enum class HotReloadEventType : uint8_t
    {
        Added = 0,
        Modified,
        Removed,
    };

    struct HotReloadEvent
    {
        std::string watchId{};
        std::filesystem::path path{};
        HotReloadEventType type{HotReloadEventType::Modified};
    };

    struct HotReloadWatchSpec
    {
        std::string id{};
        std::filesystem::path rootPath{};
        std::vector<std::string> fileExtensions{};
        bool recursive = true;
    };

    class HotReloadManager
    {
    public:
        using CallbackToken = uint64_t;
        using EventCallback = std::function<void(const HotReloadEvent &)>;

        explicit HotReloadManager(std::chrono::milliseconds pollInterval = std::chrono::milliseconds{500});
        ~HotReloadManager();

        HotReloadManager(const HotReloadManager &) = delete;
        HotReloadManager &operator=(const HotReloadManager &) = delete;
        HotReloadManager(HotReloadManager &&) = delete;
        HotReloadManager &operator=(HotReloadManager &&) = delete;

        bool addWatch(HotReloadWatchSpec watchSpec);
        bool removeWatch(std::string_view watchId);
        void clearWatches();
        std::vector<HotReloadWatchSpec> getWatches() const;

        CallbackToken subscribe(EventCallback callback);
        CallbackToken subscribe(EventCallback callback, std::vector<std::string_view> watchIds);
        bool unsubscribe(CallbackToken token);

        void start();
        void stop();
        void restart();
        bool isRunning() const { return running.load(); }

        std::vector<HotReloadEvent> consumePendingEvents();
        size_t dispatchPendingEvents();

        void setPollInterval(std::chrono::milliseconds pollInterval);
        std::chrono::milliseconds getPollInterval() const;

    private:
        struct FileState
        {
            std::filesystem::file_time_type lastWriteTime{};
            uintmax_t fileSize = 0;
        };

        struct WatchState
        {
            HotReloadWatchSpec spec{};
            std::unordered_map<std::string, FileState> knownFiles{};
        };

        std::unordered_map<std::string, FileState> scanFiles(const HotReloadWatchSpec &watchSpec) const;
        bool shouldWatchFile(const HotReloadWatchSpec &watchSpec, const std::filesystem::path &path) const;
        void workerLoop();

        mutable std::mutex mutex;
        std::string defaultWatchId = "any";
        std::condition_variable wakeCondition;
        std::unordered_map<std::string, WatchState> watches;
        std::unordered_map<CallbackToken, EventCallback> subscribers;
        std::unordered_map<std::string, std::vector<CallbackToken>> watchIdSubscribers;
        std::unordered_map<std::string, HotReloadEvent> pendingEvents;
        std::chrono::milliseconds pollInterval;
        std::thread workerThread;
        std::atomic<bool> running{false};
        bool stopRequested = false;
        CallbackToken nextCallbackToken = 1;
    };
}