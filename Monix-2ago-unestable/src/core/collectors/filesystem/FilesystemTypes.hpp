#pragma once

#include "FilesystemScope.hpp"

#include <cstdint>
#include <string>
#include <vector>
#include <filesystem>

namespace monix::collectors::fs {

enum class FilesystemEventKind : std::uint8_t {
  FileCreated,
  FileModified,
  FileDeleted,
  FileMoved,
  FileRenamed,
  DirectoryCreated,
  DirectoryDeleted,
  DirectoryMoved,
  DirectoryRenamed
};

const char* FilesystemEventKindName(FilesystemEventKind k);

struct FilesystemEvent {
  FilesystemEventKind kind = FilesystemEventKind::FileCreated;
  std::filesystem::path path;
  std::filesystem::path parent_path;
  std::string name;
  std::filesystem::path old_path;
  std::string old_name;

  std::uint64_t size = 0;
  std::uint32_t attributes = 0;
  std::int64_t creation_time = 0;
  std::int64_t modification_time = 0;
  std::int64_t access_time = 0;

  std::int64_t first_seen_ms = 0;
  std::int64_t last_seen_ms = 0;
  std::uint64_t modification_count = 1;

  std::string hash;
  std::string hash_algorithm;

  std::string event_type() const;
  bool is_file() const;
  bool is_directory() const;
};

enum class FileHashMode : std::uint8_t {
  Disabled,
  OnDemand,
  Suspicious,
  Forensic,
  Always
};

const char* FileHashModeName(FileHashMode m);
FileHashMode FileHashModeFromString(const char* s);

enum class WatcherMode : std::uint8_t {
  Native,
  Polling,
  Hybrid
};

const char* WatcherModeName(WatcherMode m);

struct FilesystemConfig {
  std::vector<std::filesystem::path> watch_paths;
  bool recursive = true;
  WatcherMode watcher_mode = WatcherMode::Native;
  std::uint32_t polling_interval_ms = 5000;
  FileHashMode hash_mode = FileHashMode::Disabled;
  std::uint32_t coalesce_window_ms = 500;
  std::uint32_t dedup_window_ms = 200;
  std::size_t max_path_length = 32767;
  bool normalize_paths = true;
  FilesystemScopeConfig scope;

  static FilesystemConfig defaults();
};

}  // namespace monix::collectors::fs
