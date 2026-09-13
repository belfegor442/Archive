#pragma once

#include "StorageTypes.hpp"

#include <functional>
#include <vector>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <unordered_map>
#include <unordered_set>

namespace monix::collectors::storage {

using StorageCallback = std::function<void(
  StorageEventKind, StorageDetectionOrigin, const StorageInfo&)>;

struct StorageCollectorConfig {
  bool track_physical_disk = true;
  bool track_partitions = true;
  bool track_volumes = true;
  bool track_external = true;
  bool track_virtual = true;
  bool auto_enumerate = true;
  std::size_t max_events = 10000;

  static StorageCollectorConfig defaults();
};

class StorageCollector {
public:
  StorageCollector();
  ~StorageCollector();

  StorageCollector(const StorageCollector&) = delete;
  StorageCollector& operator=(const StorageCollector&) = delete;

  bool start(StorageCollectorConfig config = StorageCollectorConfig::defaults());
  bool stop();
  bool isRunning() const;

  void setCallback(StorageCallback callback);

  void reportDiskConnected(const StorageInfo& info,
                           StorageDetectionOrigin origin = StorageDetectionOrigin::Manual);
  void reportDiskDisconnected(const StorageInfo& info,
                              StorageDetectionOrigin origin = StorageDetectionOrigin::Manual);
  void reportVolumeMounted(const StorageInfo& info,
                           StorageDetectionOrigin origin = StorageDetectionOrigin::Manual);
  void reportVolumeUnmounted(const StorageInfo& info,
                             StorageDetectionOrigin origin = StorageDetectionOrigin::Manual);
  void reportVolumeChanged(const StorageInfo& info,
                           StorageDetectionOrigin origin = StorageDetectionOrigin::Manual);

  std::size_t eventsEmitted() const;
  std::size_t disksTracked() const;
  std::size_t volumesTracked() const;
  std::vector<StorageInfo> currentDisks() const;
  std::vector<StorageInfo> currentVolumes() const;
  std::vector<StorageInfo> recentEvents(std::size_t count = 100) const;

  bool isDiskTracked(const std::string& device_path) const;
  bool isVolumeMounted(const std::string& drive_letter) const;

  std::vector<StorageInfo> enumerateDisks() const;
  std::vector<StorageInfo> enumerateVolumes() const;

private:
  bool shouldTrack(StorageKind kind) const;
  void emit(StorageEventKind kind, StorageDetectionOrigin origin, const StorageInfo& info);
  bool isDuplicate(const StorageInfo& info, StorageEventKind kind);
  StorageId generateId(const DiskIdentity& disk, const VolumeIdentity& vol) const;

  StorageCollectorConfig config_;
  StorageCallback callback_;
  std::unordered_map<StorageId, StorageInfo> tracked_disks_;
  std::unordered_map<StorageId, StorageInfo> tracked_volumes_;
  std::vector<StorageInfo> event_log_;
  std::unordered_set<std::string> dedup_key_set_;
  mutable std::mutex mu_;
  std::atomic<bool> running_{false};
  std::atomic<std::size_t> events_emitted_{0};
};

}  // namespace monix::collectors::storage
