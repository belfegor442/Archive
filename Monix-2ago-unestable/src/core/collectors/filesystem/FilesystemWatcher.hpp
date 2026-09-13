#pragma once

#include "FilesystemTypes.hpp"
#include "FilesystemDeduplicator.hpp"
#include "FilesystemCoalescer.hpp"

#include <atomic>
#include <thread>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <set>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace monix::collectors::fs {

using FilesystemCallback = std::function<void(FilesystemEvent)>;

class FilesystemWatcher {
public:
  FilesystemWatcher();
  ~FilesystemWatcher();

  void setCallback(FilesystemCallback cb);
  bool addPath(const std::filesystem::path& path, bool recursive = true);
  bool removePath(const std::filesystem::path& path);
  bool start(const FilesystemConfig& config);
  bool stop();
  bool isRunning() const;

  std::size_t pathCount() const;
  std::size_t eventCount() const;

private:
  void watchLoop();
  void pollLoop();
  void processNotification(const FILE_NOTIFY_INFORMATION* info,
                           const std::filesystem::path& basePath);
  void processPollResult(const std::filesystem::path& path,
                         const std::filesystem::path& name,
                         DWORD attrs, FILETIME* ft);
  void emitEvent(FilesystemEvent event);

  FilesystemCallback callback_;
  FilesystemConfig config_;
  mutable std::mutex mu_;
  std::set<std::wstring> watchPaths_;
  std::atomic<bool> running_{false};
  std::atomic<std::uint64_t> eventCount_{0};

  std::thread watchThread_;
  std::vector<HANDLE> dirHandles_;
  std::vector<std::unique_ptr<OVERLAPPED>> overlappeds_;
  std::condition_variable stopSignal_;

  FilesystemDeduplicator dedup_;
  FilesystemCoalescer coalescer_;
  FilesystemScope scope_;

  struct PollEntry {
    std::filesystem::path path;
    std::int64_t last_modification_time = 0;
    std::uint64_t last_size = 0;
    bool exists = true;
  };
  std::vector<PollEntry> pollEntries_;
};

}  // namespace monix::collectors::fs
