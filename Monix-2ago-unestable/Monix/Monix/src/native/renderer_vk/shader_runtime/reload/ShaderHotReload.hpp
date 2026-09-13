#pragma once

#include "../core/ShaderLanguage.hpp"
#include "../dependencies/ShaderDependencyGraph.hpp"

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace monix::renderer_vk {

struct ShaderReloadRequest {
    std::filesystem::path changedFile;
    std::vector<std::filesystem::path> affectedAssets;
    uint64_t requestId = 0;
    bool isPreset = false;
};

struct HotReloadConfig {
    uint32_t debounceMs = 150;
    std::vector<std::filesystem::path> watchRoots;
    bool enabled = true;
};

using ReloadCallback = std::function<void(const ShaderReloadRequest&)>;

class ShaderHotReload {
public:
    explicit ShaderHotReload(HotReloadConfig config);
    ~ShaderHotReload();

    ShaderHotReload(const ShaderHotReload&) = delete;
    ShaderHotReload& operator=(const ShaderHotReload&) = delete;

    void start();
    void stop();
    bool isRunning() const { return running_.load(); }

    void setCallback(ReloadCallback cb);
    void setDependencyGraph(ShaderDependencyGraph* graph);

    void addWatchPath(const std::filesystem::path& path);
    void removeWatchPath(const std::filesystem::path& path);

    bool checkForChanges();
    bool processPendingReloads();
    bool processReloadQueue();

    uint64_t requestIdCounter() const { return requestId_.load(); }

    bool hasPendingRequests() const;
    size_t pendingRequestCount() const;

    bool isRelevantExtension(const std::filesystem::path& path) const;
    uint64_t computeContentHash(const std::filesystem::path& path) const;
    uint64_t getFileWriteTime(const std::filesystem::path& path) const;
    void processEvent(const std::filesystem::path& path);
    void enqueueRequest(ShaderReloadRequest request);

    static HotReloadConfig defaultConfig(const std::filesystem::path& shaderRoot);

private:
    struct FileState {
        uint64_t contentHash = 0;
        uint64_t lastWriteTime = 0;
        bool exists = true;
    };

    struct PendingEvent {
        std::filesystem::path path;
        uint64_t timestamp = 0;
    };

    void watcherThread();
    bool isInsideWatchRoot(const std::filesystem::path& path) const;

    HotReloadConfig config_;
    ShaderDependencyGraph* graph_ = nullptr;

    std::thread watcherThread_;
    std::atomic<bool> running_{false};
    std::atomic<uint64_t> requestId_{0};

    mutable std::mutex stateMutex_;
    std::unordered_map<std::string, FileState> fileStates_;
    std::unordered_set<std::string> ignoredPaths_;

    mutable std::mutex callbackMutex_;
    ReloadCallback callback_;

    mutable std::mutex pendingMutex_;
    std::vector<PendingEvent> pendingEvents_;
    std::queue<ShaderReloadRequest> reloadQueue_;
    uint64_t lastProcessTime_ = 0;
};

}  // namespace monix::renderer_vk
