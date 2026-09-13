#include "FilesystemCollector.hpp"

#include "../events/EventFactory.hpp"

namespace monix::collectors {

FilesystemCollector::FilesystemCollector() {
  info_.id = id_;
  info_.name = "Filesystem Collector";
  info_.version = "1.0.0";
  info_.description = "Detects filesystem changes via native API or polling";
  info_.author = "MONIX";
}

FilesystemCollector::~FilesystemCollector() {
  stop();
}

const CollectorId& FilesystemCollector::id() const { return id_; }
const CollectorInfo& FilesystemCollector::info() const { return info_; }

CollectorStatus FilesystemCollector::status() const {
  CollectorStatus st;
  st.lifecycle = lifecycle_;
  st.last_event_count = eventsCollected_;
  return st;
}

CollectorCapability FilesystemCollector::capabilities() const {
  return CollectorCapability::Realtime | CollectorCapability::Snapshot |
         CollectorCapability::Recovery;
}

bool FilesystemCollector::start(const CollectorConfig& config) {
  if (lifecycle_ == CollectorLifecycle::Running) return false;

  lifecycle_ = CollectorLifecycle::Initializing;

  if (fsConfig_.watch_paths.empty()) {
    parseFsConfig(config);
  }

  watcher_.setCallback([this](fs::FilesystemEvent fsEvent) {
    onFilesystemEvent(std::move(fsEvent));
  });

  for (const auto& path : fsConfig_.watch_paths) {
    if (std::filesystem::exists(path)) {
      watcher_.addPath(path, fsConfig_.recursive);
    }
  }

  if (!watcher_.start(fsConfig_)) {
    lifecycle_ = CollectorLifecycle::Failed;
    return false;
  }

  lifecycle_ = CollectorLifecycle::Running;
  return true;
}

bool FilesystemCollector::stop() {
  if (lifecycle_ == CollectorLifecycle::Stopped) return false;
  if (lifecycle_ == CollectorLifecycle::Created) return false;

  lifecycle_ = CollectorLifecycle::Stopping;
  watcher_.stop();
  lifecycle_ = CollectorLifecycle::Stopped;
  return true;
}

void FilesystemCollector::setEventCallback(EventCallback callback) {
  eventCallback_ = std::move(callback);
}

const fs::FilesystemConfig& FilesystemCollector::fsConfig() const {
  return fsConfig_;
}

void FilesystemCollector::setFilesystemConfig(fs::FilesystemConfig fsCfg) {
  fsConfig_ = std::move(fsCfg);
}

std::size_t FilesystemCollector::eventsCollected() const {
  return eventsCollected_;
}

std::size_t FilesystemCollector::duplicatesSkipped() const {
  return duplicatesSkipped_;
}

std::size_t FilesystemCollector::coalescedCount() const {
  return coalescedCount_;
}

void FilesystemCollector::onFilesystemEvent(fs::FilesystemEvent fsEvent) {
  if (!eventCallback_) return;

  if (fsConfig_.hash_mode != fs::FileHashMode::Disabled &&
      fsConfig_.hash_mode != fs::FileHashMode::OnDemand) {
    if (fsEvent.is_file() && std::filesystem::exists(fsEvent.path)) {
      fsEvent.hash = fs::FilesystemHasher::hashFile(fsEvent.path, fsConfig_.hash_mode);
      fsEvent.hash_algorithm = "SHA-256";
    }
  }

  auto event = convertToEvent(fsEvent);
  eventsCollected_++;
  eventCallback_(std::move(event));
}

events::Event FilesystemCollector::convertToEvent(const fs::FilesystemEvent& fe) {
  events::SourceRef source;
  source.id = id_;
  source.name = info_.name;
  source.version = info_.version;
  source.kind = events::SourceKind::MonixCollector;

  events::Event event = events::EventFactory::createSimple(
    "filesystem",
    fe.is_file() ? "file" : "directory",
    source,
    events::EventSeverity::Info);

  std::string namePart = fe.is_file() ? "file" : "directory";
  std::string action;
  switch (fe.kind) {
    case fs::FilesystemEventKind::FileCreated:
    case fs::FilesystemEventKind::DirectoryCreated:
      action = "created"; break;
    case fs::FilesystemEventKind::FileModified:
      action = "modified"; break;
    case fs::FilesystemEventKind::FileDeleted:
    case fs::FilesystemEventKind::DirectoryDeleted:
      action = "deleted"; break;
    case fs::FilesystemEventKind::FileMoved:
    case fs::FilesystemEventKind::DirectoryMoved:
      action = "moved"; break;
    case fs::FilesystemEventKind::FileRenamed:
    case fs::FilesystemEventKind::DirectoryRenamed:
      action = "renamed"; break;
  }

  event.type = {"filesystem", fe.is_file() ? "file" : "directory"};
  event.action = events::Action{action};
  event.payload.set("path", fe.path.string());
  event.payload.set("name", fe.name);
  event.payload.set("parent", fe.parent_path.string());

  if (fe.size > 0) {
    event.payload.set("size", static_cast<std::uint64_t>(fe.size));
  }
  if (fe.modification_time > 0) {
    event.payload.set("modification_time", fe.modification_time);
  }
  if (!fe.hash.empty()) {
    event.payload.set("hash", fe.hash);
    event.payload.set("hash_algorithm", fe.hash_algorithm);
  }
  if (fe.modification_count > 1) {
    event.payload.set("first_seen_ms", fe.first_seen_ms);
    event.payload.set("last_seen_ms", fe.last_seen_ms);
    event.payload.set("modification_count",
                      static_cast<std::uint64_t>(fe.modification_count));
  }
  if (!fe.old_path.empty()) {
    event.payload.set("old_path", fe.old_path.string());
  }
  if (!fe.old_name.empty()) {
    event.payload.set("old_name", fe.old_name);
  }

  return event;
}

void FilesystemCollector::parseFsConfig(const CollectorConfig& base) {
  fsConfig_ = fs::FilesystemConfig::defaults();

  if (base.has(CollectorConfigField::SamplingIntervalMs)) {
    fsConfig_.polling_interval_ms = static_cast<std::uint32_t>(
      base.getInt(CollectorConfigField::SamplingIntervalMs, 5000));
  }

  fsConfig_.watcher_mode = fs::WatcherMode::Polling;
  fsConfig_.hash_mode = fs::FileHashMode::Disabled;
  fsConfig_.coalesce_window_ms = 500;
  fsConfig_.dedup_window_ms = 200;
  fsConfig_.recursive = true;

  if (!fsConfig_.watch_paths.empty()) {
    watcher_.setCallback([this](fs::FilesystemEvent fsEvent) {
      onFilesystemEvent(std::move(fsEvent));
    });
    for (const auto& path : fsConfig_.watch_paths) {
      if (std::filesystem::exists(path)) {
        watcher_.addPath(path, fsConfig_.recursive);
      }
    }
  }
}

}  // namespace monix::collectors
