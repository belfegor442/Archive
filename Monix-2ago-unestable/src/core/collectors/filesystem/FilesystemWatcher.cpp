#include "FilesystemWatcher.hpp"

#include <algorithm>
#include <chrono>

namespace monix::collectors::fs {

static std::int64_t filetimeToMs(const FILETIME& ft) {
  ULARGE_INTEGER li;
  li.LowPart = ft.dwLowDateTime;
  li.HighPart = ft.dwHighDateTime;
  return static_cast<std::int64_t>(li.QuadPart / 10000);
}

static std::int64_t nowMs() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::steady_clock::now().time_since_epoch()).count();
}

FilesystemWatcher::FilesystemWatcher() = default;

FilesystemWatcher::~FilesystemWatcher() {
  stop();
}

void FilesystemWatcher::setCallback(FilesystemCallback cb) {
  callback_ = std::move(cb);
}

bool FilesystemWatcher::addPath(const std::filesystem::path& path, bool) {
  std::lock_guard<std::mutex> lock(mu_);
  watchPaths_.insert(path.wstring());
  return true;
}

bool FilesystemWatcher::removePath(const std::filesystem::path& path) {
  std::lock_guard<std::mutex> lock(mu_);
  return watchPaths_.erase(path.wstring()) > 0;
}

bool FilesystemWatcher::start(const FilesystemConfig& config) {
  if (running_) return false;
  config_ = config;
  dedup_.setWindow(config.dedup_window_ms);
  coalescer_.setWindow(config.coalesce_window_ms);
  scope_ = FilesystemScope(config.scope);

  running_ = true;

  if (config_.watcher_mode == WatcherMode::Native ||
      config_.watcher_mode == WatcherMode::Hybrid) {
    for (const auto& wpath : watchPaths_) {
      HANDLE hDir = CreateFileW(
        wpath.c_str(),
        FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
        nullptr);

      if (hDir == INVALID_HANDLE_VALUE) {
        if (config_.watcher_mode == WatcherMode::Native) {
          continue;
        }
        continue;
      }

      dirHandles_.push_back(hDir);
      auto overlapped = std::make_unique<OVERLAPPED>();
      overlapped->hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
      overlappeds_.push_back(std::move(overlapped));
    }
  }

  if (config_.watcher_mode == WatcherMode::Polling ||
      config_.watcher_mode == WatcherMode::Hybrid) {
    for (const auto& wpath : watchPaths_) {
      std::filesystem::path p(wpath);
      std::error_code ec;
      if (std::filesystem::is_directory(p, ec)) {
        if (config_.recursive) {
          for (auto& entry : std::filesystem::recursive_directory_iterator(
                   p, std::filesystem::directory_options::skip_permission_denied, ec)) {
            std::error_code fec;
            auto decision = scope_.evaluate(entry.path());
            if (decision != ScopeDecision::Include) continue;
            if (entry.is_regular_file(fec)) {
              PollEntry pe;
              pe.path = entry.path();
              pe.exists = true;
              auto ftime = std::filesystem::last_write_time(pe.path, fec);
              if (!fec) {
                pe.last_modification_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                  ftime.time_since_epoch()).count();
              }
              pe.last_size = std::filesystem::file_size(pe.path, fec);
              pollEntries_.push_back(std::move(pe));
            }
          }
        } else {
          for (auto& entry : std::filesystem::directory_iterator(p)) {
            std::error_code fec;
            auto decision = scope_.evaluate(entry.path());
            if (decision != ScopeDecision::Include) continue;
            if (entry.is_regular_file(fec)) {
              PollEntry pe;
              pe.path = entry.path();
              pe.exists = true;
              auto ftime = std::filesystem::last_write_time(pe.path, fec);
              if (!fec) {
                pe.last_modification_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                  ftime.time_since_epoch()).count();
              }
              pe.last_size = std::filesystem::file_size(pe.path, fec);
              pollEntries_.push_back(std::move(pe));
            }
          }
        }
      } else if (std::filesystem::exists(p, ec)) {
        auto decision = scope_.evaluate(p);
        if (decision != ScopeDecision::Include) continue;
        PollEntry pe;
        pe.path = p;
        pe.exists = true;
        auto ftime = std::filesystem::last_write_time(p);
        pe.last_modification_time = std::chrono::duration_cast<std::chrono::milliseconds>(
          ftime.time_since_epoch()).count();
        pe.last_size = std::filesystem::file_size(p);
        pollEntries_.push_back(std::move(pe));
        }
      }

      for (const auto& wpath : watchPaths_) {
        std::filesystem::path basePath(wpath);
        std::error_code ec;
        if (!std::filesystem::is_directory(basePath, ec)) continue;

        auto scanDir = [&](const std::filesystem::path& dir) {
          for (auto& entry : std::filesystem::directory_iterator(dir, ec)) {
            std::error_code fec;
            auto decision = scope_.evaluate(entry.path());
            if (decision != ScopeDecision::Include) continue;
            if (!entry.is_regular_file(fec)) continue;
            const auto& ep = entry.path();
            bool found = false;
            for (const auto& pe : pollEntries_) {
              if (pe.path == ep) { found = true; break; }
            }
            if (!found) {
              PollEntry pe;
              pe.path = ep;
              pe.exists = false;
              pollEntries_.push_back(pe);
            }
          }
        };

        if (config_.recursive) {
          for (auto& dir : std::filesystem::recursive_directory_iterator(
                   basePath, std::filesystem::directory_options::skip_permission_denied, ec)) {
            std::error_code dec;
            if (dir.is_directory(dec)) scanDir(dir.path());
          }
        }
        scanDir(basePath);
      }
    }

  watchThread_ = std::thread(&FilesystemWatcher::watchLoop, this);
  return true;
}

bool FilesystemWatcher::stop() {
  if (!running_) return false;
  running_ = false;
  stopSignal_.notify_all();

  for (auto h : dirHandles_) {
    CancelIoEx(h, nullptr);
    CloseHandle(h);
  }
  dirHandles_.clear();

  for (auto& ov : overlappeds_) {
    if (ov->hEvent) CloseHandle(ov->hEvent);
  }
  overlappeds_.clear();

  if (watchThread_.joinable()) {
    watchThread_.join();
  }

  auto flushed = coalescer_.flush(nowMs() + 100000);
  for (auto& e : flushed) {
    emitEvent(std::move(e));
  }

  return true;
}

bool FilesystemWatcher::isRunning() const {
  return running_;
}

std::size_t FilesystemWatcher::pathCount() const {
  std::lock_guard<std::mutex> lock(mu_);
  return watchPaths_.size();
}

std::size_t FilesystemWatcher::eventCount() const {
  return eventCount_;
}

void FilesystemWatcher::watchLoop() {
  while (running_) {
    if (!dirHandles_.empty()) {
      for (std::size_t i = 0; i < dirHandles_.size() && i < overlappeds_.size(); ++i) {
        HANDLE hDir = dirHandles_[i];
        OVERLAPPED* ov = overlappeds_[i].get();

        constexpr DWORD kBufferSize = 4096;
        alignas(DWORD) char buffer[kBufferSize];

        DWORD bytesReturned = 0;
        ResetEvent(ov->hEvent);

        BOOL ok = ReadDirectoryChangesW(
          hDir, buffer, kBufferSize, FALSE,
          FILE_NOTIFY_CHANGE_FILE_NAME |
          FILE_NOTIFY_CHANGE_DIR_NAME |
          FILE_NOTIFY_CHANGE_SIZE |
          FILE_NOTIFY_CHANGE_LAST_WRITE |
          FILE_NOTIFY_CHANGE_CREATION,
          &bytesReturned, ov, nullptr);

        if (ok) {
          DWORD waitResult = WaitForSingleObject(ov->hEvent, 100);
          if (waitResult == WAIT_OBJECT_0) {
            auto* info = reinterpret_cast<FILE_NOTIFY_INFORMATION*>(buffer);
            std::lock_guard<std::mutex> lock(mu_);
            auto it = watchPaths_.begin();
            std::advance(it, i < watchPaths_.size() ? i : 0);
            std::filesystem::path basePath(*it);
            processNotification(info, basePath);
          }
        }
      }
    }

    if (config_.watcher_mode == WatcherMode::Polling ||
        config_.watcher_mode == WatcherMode::Hybrid) {
      std::lock_guard<std::mutex> lock(mu_);
      for (auto& pe : pollEntries_) {
        std::error_code ec;
        bool exists = std::filesystem::exists(pe.path, ec);
        if (ec || !exists) {
          if (pe.exists) {
            pe.exists = false;
            FilesystemEvent event;
            event.kind = pe.path.has_extension()
              ? FilesystemEventKind::FileDeleted
              : FilesystemEventKind::DirectoryDeleted;
            event.path = pe.path;
            event.parent_path = pe.path.parent_path();
            event.name = pe.path.filename().string();
            event.last_seen_ms = nowMs();
            emitEvent(std::move(event));
          }
          continue;
        }

        auto ftime = std::filesystem::last_write_time(pe.path, ec);
        if (!ec) {
          auto modTime = std::chrono::duration_cast<std::chrono::milliseconds>(
            ftime.time_since_epoch()).count();
          auto fileSize = std::filesystem::file_size(pe.path, ec);

          if (!pe.exists) {
            FilesystemEvent event;
            event.kind = pe.path.has_extension()
              ? FilesystemEventKind::FileCreated
              : FilesystemEventKind::DirectoryCreated;
            event.path = pe.path;
            event.parent_path = pe.path.parent_path();
            event.name = pe.path.filename().string();
            event.size = fileSize;
            event.modification_time = modTime;
            event.last_seen_ms = nowMs();
            pe.exists = true;
            pe.last_modification_time = modTime;
            pe.last_size = fileSize;
            emitEvent(std::move(event));
          } else if (modTime != pe.last_modification_time || fileSize != pe.last_size) {
            FilesystemEvent event;
            event.kind = pe.path.has_extension()
              ? FilesystemEventKind::FileModified
              : FilesystemEventKind::DirectoryCreated;
            event.path = pe.path;
            event.parent_path = pe.path.parent_path();
            event.name = pe.path.filename().string();
            event.size = fileSize;
            event.modification_time = modTime;
            event.last_seen_ms = nowMs();
            pe.last_modification_time = modTime;
            pe.last_size = fileSize;
            emitEvent(std::move(event));
          }
        }
      }

      for (const auto& wpath : watchPaths_) {
        std::filesystem::path basePath(wpath);
        std::error_code scanEc;
        if (!std::filesystem::is_directory(basePath, scanEc)) continue;

        auto scanDir = [&](const std::filesystem::path& dir) {
          std::error_code dirEc;
          for (auto& entry : std::filesystem::directory_iterator(dir, dirEc)) {
            std::error_code fec;
            auto decision = scope_.evaluate(entry.path());
            if (decision != ScopeDecision::Include) continue;
            bool isFile = entry.is_regular_file(fec);
            bool isDir = !isFile && entry.is_directory(fec);
            if (!isFile && !isDir) continue;
            const auto& ep = entry.path();
            bool found = false;
            for (const auto& pe : pollEntries_) {
              if (pe.path == ep) { found = true; break; }
            }
            if (!found) {
              PollEntry pe;
              pe.path = ep;
              pe.exists = false;
              pollEntries_.push_back(pe);
            }
          }
        };

        if (config_.recursive) {
          for (auto& dir : std::filesystem::recursive_directory_iterator(
                   basePath, std::filesystem::directory_options::skip_permission_denied, scanEc)) {
            std::error_code dec;
            if (dir.is_directory(dec)) scanDir(dir.path());
          }
        }
        scanDir(basePath);
      }
    }

    auto flushed = coalescer_.flush(nowMs());
    for (auto& e : flushed) {
      emitEvent(std::move(e));
    }

    std::unique_lock<std::mutex> lock(mu_);
    stopSignal_.wait_for(lock, std::chrono::milliseconds(config_.polling_interval_ms),
                         [this] { return !running_; });
  }
}

void FilesystemWatcher::processNotification(const FILE_NOTIFY_INFORMATION* info,
                                             const std::filesystem::path& basePath) {
  const FILE_NOTIFY_INFORMATION* current = info;
  do {
    std::wstring fileName(current->FileName,
                          current->FileNameLength / sizeof(WCHAR));
    std::filesystem::path fullPath = basePath / fileName;

    auto decision = scope_.evaluate(fullPath);
    if (decision != ScopeDecision::Include) {
      if (current->NextEntryOffset == 0) break;
      current = reinterpret_cast<const FILE_NOTIFY_INFORMATION*>(
        reinterpret_cast<const char*>(current) + current->NextEntryOffset);
      continue;
    }

    bool isDir = std::filesystem::is_directory(fullPath);

    FilesystemEvent event;
    event.path = fullPath;
    event.parent_path = basePath;
    event.name = fullPath.filename().string();
    event.size = current->FileNameLength;
    event.last_seen_ms = nowMs();

    switch (current->Action) {
      case FILE_ACTION_ADDED:
        event.kind = isDir ? FilesystemEventKind::DirectoryCreated
                           : FilesystemEventKind::FileCreated;
        break;
      case FILE_ACTION_REMOVED:
        event.kind = isDir ? FilesystemEventKind::DirectoryDeleted
                           : FilesystemEventKind::FileDeleted;
        break;
      case FILE_ACTION_MODIFIED:
        event.kind = isDir ? FilesystemEventKind::DirectoryCreated
                           : FilesystemEventKind::FileModified;
        break;
      case FILE_ACTION_RENAMED_OLD_NAME:
        event.kind = isDir ? FilesystemEventKind::DirectoryRenamed
                           : FilesystemEventKind::FileRenamed;
        event.old_name = event.name;
        event.old_path = event.path;
        break;
      case FILE_ACTION_RENAMED_NEW_NAME:
        event.kind = isDir ? FilesystemEventKind::DirectoryRenamed
                           : FilesystemEventKind::FileRenamed;
        break;
      default:
        event.kind = FilesystemEventKind::FileModified;
        break;
    }

    if (!dedup_.isDuplicate(event, nowMs())) {
      if (config_.coalesce_window_ms > 0) {
        coalescer_.addEvent(std::move(event), nowMs());
      } else {
        emitEvent(std::move(event));
      }
    } else {
      eventCount_++;
    }

    if (current->NextEntryOffset == 0) break;
    current = reinterpret_cast<const FILE_NOTIFY_INFORMATION*>(
      reinterpret_cast<const char*>(current) + current->NextEntryOffset);
  } while (true);
}

void FilesystemWatcher::emitEvent(FilesystemEvent event) {
  eventCount_++;
  if (callback_) {
    callback_(std::move(event));
  }
}

}  // namespace monix::collectors::fs
