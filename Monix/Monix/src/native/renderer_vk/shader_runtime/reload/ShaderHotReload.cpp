#include "ShaderHotReload.hpp"

#include "../../core/FileSystem.hpp"
#include "../../core/Hash.hpp"

#include <algorithm>
#include <chrono>
#include <fstream>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#undef min
#undef max

namespace monix::renderer_vk {

ShaderHotReload::ShaderHotReload(HotReloadConfig config)
    : config_(std::move(config)) {}

ShaderHotReload::~ShaderHotReload() {
    stop();
}

void ShaderHotReload::setCallback(ReloadCallback cb) {
    std::lock_guard lock(callbackMutex_);
    callback_ = std::move(cb);
}

void ShaderHotReload::setDependencyGraph(ShaderDependencyGraph* graph) {
    graph_ = graph;
}

void ShaderHotReload::addWatchPath(const std::filesystem::path& path) {
    std::lock_guard lock(stateMutex_);
    config_.watchRoots.push_back(path);
}

void ShaderHotReload::removeWatchPath(const std::filesystem::path& path) {
    std::lock_guard lock(stateMutex_);
    auto key = FileSystem::normalize(path).string();
    config_.watchRoots.erase(
        std::remove_if(config_.watchRoots.begin(), config_.watchRoots.end(),
            [&](const auto& p) { return FileSystem::normalize(p).string() == key; }),
        config_.watchRoots.end());
}

void ShaderHotReload::start() {
    if (running_.load()) return;
    running_.store(true);
    watcherThread_ = std::thread(&ShaderHotReload::watcherThread, this);
}

void ShaderHotReload::stop() {
    running_.store(false);
    if (watcherThread_.joinable()) {
        watcherThread_.join();
    }
}

uint64_t ShaderHotReload::computeContentHash(const std::filesystem::path& path) const {
    auto text = FileSystem::readText(path);
    if (!text) return 0;
    return Hash::fnv1a(text.value());
}

uint64_t ShaderHotReload::getFileWriteTime(const std::filesystem::path& path) const {
    std::error_code ec;
    auto ftime = std::filesystem::last_write_time(path, ec);
    if (ec) return 0;
    auto duration = ftime.time_since_epoch();
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(duration).count());
}

bool ShaderHotReload::isRelevantExtension(const std::filesystem::path& path) const {
    auto ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ext == ".slang" || ext == ".glsl" || ext == ".vert" ||
           ext == ".frag" || ext == ".geom" || ext == ".comp" ||
           ext == ".slangp" || ext == ".glslp" || ext == ".inc";
}

bool ShaderHotReload::isInsideWatchRoot(const std::filesystem::path& path) const {
    std::lock_guard lock(stateMutex_);
    if (config_.watchRoots.empty()) return true;
    std::error_code ec;
    auto absPath = std::filesystem::absolute(path, ec);
    if (ec) return false;
    for (const auto& root : config_.watchRoots) {
        auto absRoot = std::filesystem::absolute(root, ec);
        if (ec) continue;
        auto rel = absPath.string();
        auto rootStr = absRoot.string();
        if (rel.size() >= rootStr.size()) {
            if (_strnicmp(rel.c_str(), rootStr.c_str(), rootStr.size()) == 0) {
                return true;
            }
        }
    }
    return false;
}

void ShaderHotReload::processEvent(const std::filesystem::path& path) {
    if (!isRelevantExtension(path)) return;
    if (!isInsideWatchRoot(path)) return;

    auto normalized = FileSystem::normalize(path);
    auto key = normalized.string();

    {
        std::lock_guard lock(stateMutex_);
        if (ignoredPaths_.count(key)) return;
    }

    auto now = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());

    uint64_t newHash = computeContentHash(normalized);
    uint64_t newWriteTime = getFileWriteTime(normalized);

    {
        std::lock_guard lock(stateMutex_);
        auto it = fileStates_.find(key);
        if (it != fileStates_.end()) {
            if (it->second.contentHash == newHash && it->second.lastWriteTime == newWriteTime) {
                return;
            }
            if (!std::filesystem::exists(normalized)) {
                it->second.exists = false;
            } else {
                it->second.contentHash = newHash;
                it->second.lastWriteTime = newWriteTime;
            }
        } else {
            FileState state;
            state.contentHash = newHash;
            state.lastWriteTime = newWriteTime;
            state.exists = std::filesystem::exists(normalized);
            fileStates_[key] = state;
        }
    }

    {
        std::lock_guard lock(pendingMutex_);
        pendingEvents_.push_back({normalized, now});
    }
}

bool ShaderHotReload::checkForChanges() {
    std::vector<std::filesystem::path> rootsCopy;
    {
        std::lock_guard lock(stateMutex_);
        rootsCopy = config_.watchRoots;
    }
    if (rootsCopy.empty()) return false;

    bool foundChanges = false;

    for (const auto& root : rootsCopy) {
        std::error_code ec;
        if (!std::filesystem::exists(root, ec)) continue;

        for (auto it = std::filesystem::recursive_directory_iterator(
                 root, std::filesystem::directory_options::skip_permission_denied, ec);
             it != std::filesystem::recursive_directory_iterator(); ++it) {
            if (ec) break;
            if (it->is_regular_file()) {
                processEvent(it->path());
                foundChanges = true;
            }
        }
    }

    return foundChanges;
}

bool ShaderHotReload::processPendingReloads() {
    auto now = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());

    std::vector<PendingEvent> eventsToProcess;
    {
        std::lock_guard lock(pendingMutex_);
        if (pendingEvents_.empty()) {
            eventsToProcess.clear();
        } else {
            if (now - lastProcessTime_ < config_.debounceMs) return false;

            eventsToProcess = std::move(pendingEvents_);
            pendingEvents_.clear();
            lastProcessTime_ = now;
        }
    }

    if (eventsToProcess.empty()) return processReloadQueue();

    std::unordered_set<std::string> uniquePaths;
    for (const auto& event : eventsToProcess) {
        uniquePaths.insert(FileSystem::normalize(event.path).string());
    }

    for (const auto& pathStr : uniquePaths) {
        std::filesystem::path path(pathStr);
        ShaderReloadRequest request;
        request.changedFile = path;
        request.requestId = requestId_.fetch_add(1, std::memory_order_relaxed) + 1;

        bool isPreset = false;
        auto ext = path.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (ext == ".slangp" || ext == ".glslp") {
            isPreset = true;
            request.affectedAssets.push_back(path);
        }

        if (graph_) {
            auto affected = graph_->invalidate(path);
            for (const auto& p : affected.affectedPresets) {
                if (std::find(request.affectedAssets.begin(), request.affectedAssets.end(), p)
                    == request.affectedAssets.end()) {
                    request.affectedAssets.push_back(p);
                }
                isPreset = true;
            }
            for (const auto& p : affected.affectedShaders) {
                if (std::find(request.affectedAssets.begin(), request.affectedAssets.end(), p)
                    == request.affectedAssets.end()) {
                    request.affectedAssets.push_back(p);
                }
            }
        }

        request.isPreset = isPreset;
        enqueueRequest(std::move(request));
    }

    processReloadQueue();
    return true;
}

bool ShaderHotReload::processReloadQueue() {
    std::queue<ShaderReloadRequest> toProcess;
    {
        std::lock_guard lock(pendingMutex_);
        if (reloadQueue_.empty()) return false;
        toProcess = std::move(reloadQueue_);
        reloadQueue_ = std::queue<ShaderReloadRequest>();
    }

    while (!toProcess.empty()) {
        {
            std::lock_guard lock(callbackMutex_);
            if (callback_) {
                callback_(toProcess.front());
            }
        }
        toProcess.pop();
    }
    return true;
}

void ShaderHotReload::enqueueRequest(ShaderReloadRequest request) {
    if (request.requestId == 0) {
        request.requestId = requestId_.fetch_add(1, std::memory_order_relaxed) + 1;
    } else {
        auto observed = requestId_.load(std::memory_order_relaxed);
        while (observed < request.requestId &&
               !requestId_.compare_exchange_weak(
                   observed, request.requestId, std::memory_order_relaxed)) {
        }
    }
    std::lock_guard lock(pendingMutex_);
    reloadQueue_.push(std::move(request));
}

bool ShaderHotReload::hasPendingRequests() const {
    std::lock_guard lock(pendingMutex_);
    return !reloadQueue_.empty();
}

size_t ShaderHotReload::pendingRequestCount() const {
    std::lock_guard lock(pendingMutex_);
    return reloadQueue_.size();
}

void ShaderHotReload::watcherThread() {
    std::vector<std::filesystem::path> rootsCopy;
    {
        std::lock_guard lock(stateMutex_);
        rootsCopy = config_.watchRoots;
    }
    if (rootsCopy.empty()) return;

    std::vector<HANDLE> dirHandles;
    std::vector<std::wstring> dirPaths;

    for (const auto& root : rootsCopy) {
        std::error_code ec;
        if (!std::filesystem::exists(root, ec)) continue;

        HANDLE hDir = CreateFileW(
            root.wstring().c_str(),
            FILE_LIST_DIRECTORY,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr,
            OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
            nullptr);

        if (hDir != INVALID_HANDLE_VALUE) {
            dirHandles.push_back(hDir);
            dirPaths.push_back(root.wstring());
        }
    }

    if (dirHandles.empty()) return;

    std::vector<OVERLAPPED> overlaps(dirHandles.size());
    std::vector<std::vector<BYTE>> buffers(dirHandles.size(), std::vector<BYTE>(4096));
    std::vector<BOOL> results(dirHandles.size());

    for (size_t i = 0; i < dirHandles.size(); ++i) {
        overlaps[i] = {};
        overlaps[i].hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        results[i] = ReadDirectoryChangesW(
            dirHandles[i], buffers[i].data(),
            static_cast<DWORD>(buffers[i].size()),
            TRUE,
            FILE_NOTIFY_CHANGE_FILE_NAME |
            FILE_NOTIFY_CHANGE_LAST_WRITE |
            FILE_NOTIFY_CHANGE_CREATION,
            nullptr, &overlaps[i], nullptr);
    }

    while (running_.load()) {
        DWORD waitResult = WaitForMultipleObjects(
            static_cast<DWORD>(dirHandles.size()), &overlaps[0].hEvent,
            FALSE, 100);

        if (waitResult == WAIT_TIMEOUT) continue;
        if (waitResult == WAIT_FAILED) break;

        DWORD index = waitResult - WAIT_OBJECT_0;
        if (index >= dirHandles.size()) continue;

        DWORD bytesReturned = 0;
        if (GetOverlappedResult(dirHandles[index], &overlaps[index],
                                &bytesReturned, FALSE)) {
            if (bytesReturned > 0) {
                auto* info = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(
                    buffers[index].data());
                while (true) {
                    std::wstring fileName(info->FileName,
                        info->FileNameLength / sizeof(WCHAR));
                    std::filesystem::path fullPath =
                        std::filesystem::path(dirPaths[index]) / fileName;

                    processEvent(fullPath);

                    if (info->NextEntryOffset == 0) break;
                    info = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(
                        reinterpret_cast<BYTE*>(info) + info->NextEntryOffset);
                }
            }
        }

        ResetEvent(overlaps[index].hEvent);
        results[index] = ReadDirectoryChangesW(
            dirHandles[index], buffers[index].data(),
            static_cast<DWORD>(buffers[index].size()),
            TRUE,
            FILE_NOTIFY_CHANGE_FILE_NAME |
            FILE_NOTIFY_CHANGE_LAST_WRITE |
            FILE_NOTIFY_CHANGE_CREATION,
            nullptr, &overlaps[index], nullptr);
    }

    for (size_t i = 0; i < dirHandles.size(); ++i) {
        CancelIo(dirHandles[i]);
        CloseHandle(overlaps[i].hEvent);
        CloseHandle(dirHandles[i]);
    }
}

HotReloadConfig ShaderHotReload::defaultConfig(const std::filesystem::path& shaderRoot) {
    HotReloadConfig config;
    config.debounceMs = 150;
    config.watchRoots.push_back(shaderRoot);
    config.enabled = true;
    return config;
}

}  // namespace monix::renderer_vk
