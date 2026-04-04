#include "Core/HotReload/HotReloadManager.hpp"

#include <algorithm>
#include <ranges>
#include "quill/LogMacros.h"

using namespace Faye;

namespace
{
    std::string normalizeExtension(std::string extension)
    {
        std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char character)
                       { return static_cast<char>(std::tolower(character)); });

        if (!extension.empty() && extension.front() != '.')
        {
            extension.insert(extension.begin(), '.');
        }

        return extension;
    }

    std::string eventKey(const HotReloadEvent &event)
    {
        return event.watchId + "|" + event.path.lexically_normal().string();
    }
}

HotReloadManager::HotReloadManager(std::chrono::milliseconds interval)
    : pollInterval(interval)
{
}

HotReloadManager::~HotReloadManager()
{
    stop();
}

bool HotReloadManager::addWatch(HotReloadWatchSpec watchSpec)
{
    if (watchSpec.id.empty() || watchSpec.rootPath.empty())
    {
        return false;
    }

    for (auto &extension : watchSpec.fileExtensions)
    {
        extension = normalizeExtension(std::move(extension));
    }

    WatchState watchState{};
    watchState.spec = std::move(watchSpec);
    watchState.knownFiles = scanFiles(watchState.spec);

    {
        std::lock_guard<std::mutex> lock{mutex};
        watches[watchState.spec.id] = std::move(watchState);
    }

    wakeCondition.notify_all();
    return true;
}

bool HotReloadManager::removeWatch(std::string_view watchId)
{
    std::lock_guard<std::mutex> lock{mutex};
    return watches.erase(std::string(watchId)) > 0;
}

void HotReloadManager::clearWatches()
{
    std::lock_guard<std::mutex> lock{mutex};
    watches.clear();
}

std::vector<HotReloadWatchSpec> HotReloadManager::getWatches() const
{
    std::lock_guard<std::mutex> lock{mutex};

    std::vector<HotReloadWatchSpec> result;
    result.reserve(watches.size());
    for (const auto &[_, watchState] : watches)
    {
        result.push_back(watchState.spec);
    }

    return result;
}

HotReloadManager::CallbackToken HotReloadManager::subscribe(EventCallback callback)
{
    std::lock_guard<std::mutex> lock{mutex};

    const CallbackToken token = nextCallbackToken++;
    subscribers.emplace(token, std::move(callback));
    watchIdSubscribers[defaultWatchId].push_back(token);
    return token;
}

HotReloadManager::CallbackToken HotReloadManager::subscribe(EventCallback callback, std::vector<std::string_view> watchIds)
{
    std::lock_guard<std::mutex> lock{mutex};

    const CallbackToken token = nextCallbackToken++;
    subscribers.emplace(token, std::move(callback));
    for (const auto &watchId : watchIds)
    {
        watchIdSubscribers[std::string(watchId)].push_back(token);
    }
    return token;
}

bool HotReloadManager::unsubscribe(CallbackToken token)
{
    std::lock_guard<std::mutex> lock{mutex};

    // First remove from any specific watcher lists.
    // This ensures that if the same callback token is reused after unsubscription,
    // it won't accidentally remain subscribed to old watchIds.
    for (auto &[_, tokens] : watchIdSubscribers)
    {
        auto it = std::find(tokens.begin(), tokens.end(), token);
        if (it != tokens.end())
        {
            tokens.erase(it);
        }
    }
    return subscribers.erase(token) > 0;
}

void HotReloadManager::start()
{
    bool expected = false;
    if (!running.compare_exchange_strong(expected, true))
    {
        return;
    }

    {
        std::lock_guard<std::mutex> lock{mutex};
        stopRequested = false;
    }

    workerThread = std::thread(&HotReloadManager::workerLoop, this);
}

void HotReloadManager::stop()
{
    bool expected = true;
    if (!running.compare_exchange_strong(expected, false))
    {
        return;
    }

    {
        std::lock_guard<std::mutex> lock{mutex};
        stopRequested = true;
    }

    wakeCondition.notify_all();

    if (workerThread.joinable())
    {
        workerThread.join();
    }
}

void HotReloadManager::restart()
{
    stop();
    start();
}

std::vector<HotReloadEvent> HotReloadManager::consumePendingEvents()
{
    std::lock_guard<std::mutex> lock{mutex};

    std::vector<HotReloadEvent> events;
    events.reserve(pendingEvents.size());
    for (auto &[_, event] : pendingEvents)
    {
        events.push_back(std::move(event));
    }

    pendingEvents.clear();

    std::ranges::sort(events, [](const HotReloadEvent &left, const HotReloadEvent &right)
                      {
                            if (left.watchId != right.watchId)
                            {
                                return left.watchId < right.watchId;
                            }

                            return left.path.lexically_normal().string() < right.path.lexically_normal().string(); });

    return events;
}

size_t HotReloadManager::dispatchPendingEvents()
{
    std::unordered_map<CallbackToken, EventCallback> subscribersSnapshot;
    {
        std::lock_guard<std::mutex> lock{mutex};
        subscribersSnapshot = subscribers;
    }

    std::vector<HotReloadEvent> events = consumePendingEvents();
    for (const auto &event : events)
    {
        // TODO: Only invoke callbacks for subscribers interested in the specific watchId or file path that changed
        for (const auto &token : watchIdSubscribers[event.watchId])
        {
            if (subscribersSnapshot.contains(token))
            {
                subscribersSnapshot[token](event);
            }
        }
        for (const auto &token : watchIdSubscribers[defaultWatchId])
        {
            if (subscribersSnapshot.contains(token))
            {
                subscribersSnapshot[token](event);
            }
        }
    }

    return events.size();
}

void HotReloadManager::setPollInterval(std::chrono::milliseconds interval)
{
    {
        std::lock_guard<std::mutex> lock{mutex};
        pollInterval = interval;
    }

    wakeCondition.notify_all();
}

std::chrono::milliseconds HotReloadManager::getPollInterval() const
{
    std::lock_guard<std::mutex> lock{mutex};
    return pollInterval;
}

std::unordered_map<std::string, HotReloadManager::FileState> HotReloadManager::scanFiles(const HotReloadWatchSpec &watchSpec) const
{
    std::unordered_map<std::string, FileState> files;
    std::error_code errorCode;

    if (!std::filesystem::exists(watchSpec.rootPath, errorCode))
    {
        return files;
    }

    auto visitFile = [&](const std::filesystem::directory_entry &entry)
    {
        if (!entry.is_regular_file())
        {
            return;
        }

        if (!shouldWatchFile(watchSpec, entry.path()))
        {
            return;
        }

        FileState state{};
        state.lastWriteTime = entry.last_write_time(errorCode);
        state.fileSize = entry.file_size(errorCode);
        files.emplace(entry.path().lexically_normal().string(), state);
    };

    if (watchSpec.recursive)
    {
        for (const auto &entry : std::filesystem::recursive_directory_iterator(watchSpec.rootPath, std::filesystem::directory_options::skip_permission_denied, errorCode))
        {
            if (errorCode)
            {
                errorCode.clear();
                continue;
            }

            visitFile(entry);
        }
    }
    else
    {
        for (const auto &entry : std::filesystem::directory_iterator(watchSpec.rootPath, std::filesystem::directory_options::skip_permission_denied, errorCode))
        {
            if (errorCode)
            {
                errorCode.clear();
                continue;
            }

            visitFile(entry);
        }
    }

    return files;
}

bool HotReloadManager::shouldWatchFile(const HotReloadWatchSpec &watchSpec, const std::filesystem::path &path) const
{
    if (watchSpec.fileExtensions.empty())
    {
        return true;
    }

    const std::string extension = normalizeExtension(path.extension().string());
    return std::find(watchSpec.fileExtensions.begin(), watchSpec.fileExtensions.end(), extension) != watchSpec.fileExtensions.end();
}

void HotReloadManager::workerLoop()
{
    while (running.load())
    {
        std::unordered_map<std::string, WatchState> watchesSnapshot;
        std::unordered_map<CallbackToken, EventCallback> subscribersSnapshot;
        std::chrono::milliseconds currentPollInterval{};

        {
            std::unique_lock<std::mutex> lock{mutex};
            currentPollInterval = pollInterval;
            wakeCondition.wait_for(lock, currentPollInterval, [&]
                                   { return stopRequested || !running.load(); });

            if (stopRequested || !running.load())
            {
                break;
            }

            watchesSnapshot = watches;
            subscribersSnapshot = subscribers;
        }

        std::unordered_map<std::string, std::unordered_map<std::string, FileState>> scannedFiles;
        std::vector<HotReloadEvent> events;

        for (const auto &[watchId, watchState] : watchesSnapshot)
        {
            auto currentFiles = scanFiles(watchState.spec);
            scannedFiles.emplace(watchId, currentFiles);

            for (const auto &[path, fileState] : currentFiles)
            {
                const auto knownIterator = watchState.knownFiles.find(path);
                if (knownIterator == watchState.knownFiles.end())
                {
                    events.push_back(HotReloadEvent{watchId, path, HotReloadEventType::Added});
                    continue;
                }

                if (knownIterator->second.lastWriteTime != fileState.lastWriteTime || knownIterator->second.fileSize != fileState.fileSize)
                {
                    events.push_back(HotReloadEvent{watchId, path, HotReloadEventType::Modified});
                }
            }

            for (const auto &[path, _] : watchState.knownFiles)
            {
                if (!currentFiles.contains(path))
                {
                    events.push_back(HotReloadEvent{watchId, path, HotReloadEventType::Removed});
                }
            }
        }

        {
            std::lock_guard<std::mutex> lock{mutex};
            for (auto &[watchId, fileStates] : scannedFiles)
            {
                auto watchIterator = watches.find(watchId);
                if (watchIterator != watches.end())
                {
                    watchIterator->second.knownFiles = std::move(fileStates);
                }
            }
        }

        std::lock_guard<std::mutex> lock{mutex};
        for (const auto &event : events)
        {
            pendingEvents[eventKey(event)] = event;
        }
    }
}
