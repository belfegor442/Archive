#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "SnapshotTypes.hpp"

namespace monix::collectors::snapshot {

using SnapshotCallback = std::function<void(const SnapshotEvent&)>;

class SnapshotEngine {
public:
  SnapshotEngine();
  ~SnapshotEngine();

  SnapshotEngine(const SnapshotEngine&) = delete;
  SnapshotEngine& operator=(const SnapshotEngine&) = delete;

  void setCallback(SnapshotCallback cb);

  SnapshotId beginSnapshot(const std::string& collector_name, SnapshotKind kind,
    std::size_t items_expected = 0, const std::string& description = "");

  void updateProgress(SnapshotId id, std::size_t items_captured, std::size_t items_failed = 0);

  bool completeSnapshot(SnapshotId id);
  bool failSnapshot(SnapshotId id, const std::string& error = "");

  SnapshotMeta getSnapshot(SnapshotId id) const;
  std::vector<SnapshotMeta> activeSnapshots() const;
  std::vector<SnapshotMeta> completedSnapshots() const;
  std::vector<SnapshotMeta> allSnapshots() const;

  std::size_t activeCount() const;
  std::size_t totalSnapshots() const;
  std::size_t eventsEmitted() const;

  bool isActive(SnapshotId id) const;

private:
  SnapshotId nextId();
  void emit(SnapshotEventKind kind, const SnapshotMeta& meta);

  SnapshotCallback callback_;
  mutable std::mutex mu_;
  std::unordered_map<SnapshotId, SnapshotMeta> snapshots_;
  std::atomic<std::uint64_t> next_id_{1};
  std::atomic<std::size_t> events_emitted_{0};
};

}  // namespace monix::collectors::snapshot
