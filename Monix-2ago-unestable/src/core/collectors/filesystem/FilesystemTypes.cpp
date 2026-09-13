#include "FilesystemTypes.hpp"

namespace monix::collectors::fs {

const char* FilesystemEventKindName(FilesystemEventKind k) {
  switch (k) {
    case FilesystemEventKind::FileCreated:        return "FileCreated";
    case FilesystemEventKind::FileModified:       return "FileModified";
    case FilesystemEventKind::FileDeleted:        return "FileDeleted";
    case FilesystemEventKind::FileMoved:          return "FileMoved";
    case FilesystemEventKind::FileRenamed:        return "FileRenamed";
    case FilesystemEventKind::DirectoryCreated:   return "DirectoryCreated";
    case FilesystemEventKind::DirectoryDeleted:   return "DirectoryDeleted";
    case FilesystemEventKind::DirectoryMoved:     return "DirectoryMoved";
    case FilesystemEventKind::DirectoryRenamed:   return "DirectoryRenamed";
  }
  return "Unknown";
}

std::string FilesystemEvent::event_type() const {
  switch (kind) {
    case FilesystemEventKind::FileCreated:      return "filesystem.file.created";
    case FilesystemEventKind::FileModified:     return "filesystem.file.modified";
    case FilesystemEventKind::FileDeleted:      return "filesystem.file.deleted";
    case FilesystemEventKind::FileMoved:        return "filesystem.file.moved";
    case FilesystemEventKind::FileRenamed:      return "filesystem.file.renamed";
    case FilesystemEventKind::DirectoryCreated: return "filesystem.directory.created";
    case FilesystemEventKind::DirectoryDeleted: return "filesystem.directory.deleted";
    case FilesystemEventKind::DirectoryMoved:   return "filesystem.directory.moved";
    case FilesystemEventKind::DirectoryRenamed: return "filesystem.directory.renamed";
  }
  return "filesystem.unknown";
}

bool FilesystemEvent::is_file() const {
  return kind == FilesystemEventKind::FileCreated ||
         kind == FilesystemEventKind::FileModified ||
         kind == FilesystemEventKind::FileDeleted ||
         kind == FilesystemEventKind::FileMoved ||
         kind == FilesystemEventKind::FileRenamed;
}

bool FilesystemEvent::is_directory() const {
  return kind == FilesystemEventKind::DirectoryCreated ||
         kind == FilesystemEventKind::DirectoryDeleted ||
         kind == FilesystemEventKind::DirectoryMoved ||
         kind == FilesystemEventKind::DirectoryRenamed;
}

const char* FileHashModeName(FileHashMode m) {
  switch (m) {
    case FileHashMode::Disabled:  return "Disabled";
    case FileHashMode::OnDemand:  return "OnDemand";
    case FileHashMode::Suspicious: return "Suspicious";
    case FileHashMode::Forensic:  return "Forensic";
    case FileHashMode::Always:    return "Always";
  }
  return "Unknown";
}

FileHashMode FileHashModeFromString(const char* s) {
  std::string str(s);
  if (str == "Disabled")  return FileHashMode::Disabled;
  if (str == "OnDemand")  return FileHashMode::OnDemand;
  if (str == "Suspicious") return FileHashMode::Suspicious;
  if (str == "Forensic")  return FileHashMode::Forensic;
  if (str == "Always")    return FileHashMode::Always;
  return FileHashMode::Disabled;
}

const char* WatcherModeName(WatcherMode m) {
  switch (m) {
    case WatcherMode::Native:  return "Native";
    case WatcherMode::Polling: return "Polling";
    case WatcherMode::Hybrid:  return "Hybrid";
  }
  return "Unknown";
}

FilesystemConfig FilesystemConfig::defaults() {
  FilesystemConfig cfg;
  cfg.recursive = true;
  cfg.watcher_mode = WatcherMode::Native;
  cfg.polling_interval_ms = 5000;
  cfg.hash_mode = FileHashMode::Disabled;
  cfg.coalesce_window_ms = 500;
  cfg.dedup_window_ms = 200;
  cfg.max_path_length = 32767;
  cfg.normalize_paths = true;
  return cfg;
}

}  // namespace monix::collectors::fs
