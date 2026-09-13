#include "SnapshotEngine.hpp"

namespace monix::collectors::snapshot {

SnapshotEngine::SnapshotEngine() {}

SnapshotEngine::~SnapshotEngine() {}

void SnapshotEngine::setCallback(SnapshotCallback cb) {
  std::lock_guard<std::mutex> lock(mu_);
  callback_ = std::move(cb);
}

SnapshotId SnapshotEngine::nextId() {
  return next_id_.fetch_add(1);
}

SnapshotId SnapshotEngine::beginSnapshot(const std::string& collector_name, SnapshotKind kind,
    std::size_t items_expected, const std::string& description) {

  SnapshotMeta meta;
  meta.id = nextId();
  meta.kind = kind;
  meta.state = SnapshotState::Running;
  meta.collector_name = collector_name;
  meta.description = description;
  meta.items_expected = items_expected;
  meta.items_captured = 0;
  meta.items_failed = 0;
  meta.started_at_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::system_clock::now().time_since_epoch()).count();

  {
    std::lock_guard<std::mutex> lock(mu_);
    snapshots_[meta.id] = meta;
  }

  emit(SnapshotEventKind::Started, meta);
  return meta.id;
}

void SnapshotEngine::updateProgress(SnapshotId id, std::size_t items_captured, std::size_t items_failed) {
  std::lock_guard<std::mutex> lock(mu_);
  auto it = snapshots_.find(id);
  if (it == snapshots_.end()) return;
  if (it->second.state != SnapshotState::Running) return;
  it->second.items_captured = items_captured;
  it->second.items_failed = items_failed;
}

bool SnapshotEngine::completeSnapshot(SnapshotId id) {
  SnapshotMeta meta;
  {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = snapshots_.find(id);
    if (it == snapshots_.end()) return false;
    if (it->second.state != SnapshotState::Running) return false;

    it->second.state = (it->second.items_failed > 0 && it->second.items_captured > 0)
      ? SnapshotState::Partial : SnapshotState::Completed;
    it->second.completed_at_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count();
    meta = it->second;
  }

  SnapshotEventKind kind = (meta.state == SnapshotState::Partial)
    ? SnapshotEventKind::Completed : SnapshotEventKind::Completed;
  emit(kind, meta);
  return true;
}

bool SnapshotEngine::failSnapshot(SnapshotId id, const std::string& error) {
  SnapshotMeta meta;
  {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = snapshots_.find(id);
    if (it == snapshots_.end()) return false;
    if (it->second.state != SnapshotState::Running) return false;

    it->second.state = SnapshotState::Failed;
    it->second.error_message = error;
    it->second.completed_at_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count();
    meta = it->second;
  }

  emit(SnapshotEventKind::Failed, meta);
  return true;
}

SnapshotMeta SnapshotEngine::getSnapshot(SnapshotId id) const {
  std::lock_guard<std::mutex> lock(mu_);
  auto it = snapshots_.find(id);
  if (it == snapshots_.end()) return SnapshotMeta{};
  return it->second;
}

std::vector<SnapshotMeta> SnapshotEngine::activeSnapshots() const {
  std::lock_guard<std::mutex> lock(mu_);
  std::vector<SnapshotMeta> result;
  for (const auto& [id, meta] : snapshots_) {
    if (meta.state == SnapshotState::Running || meta.state == SnapshotState::Pending) {
      result.push_back(meta);
    }
  }
  return result;
}

std::vector<SnapshotMeta> SnapshotEngine::completedSnapshots() const {
  std::lock_guard<std::mutex> lock(mu_);
  std::vector<SnapshotMeta> result;
  for (const auto& [id, meta] : snapshots_) {
    if (meta.state == SnapshotState::Completed || meta.state == SnapshotState::Partial) {
      result.push_back(meta);
    }
  }
  return result;
}

std::vector<SnapshotMeta> SnapshotEngine::allSnapshots() const {
  std::lock_guard<std::mutex> lock(mu_);
  std::vector<SnapshotMeta> result;
  result.reserve(snapshots_.size());
  for (const auto& [id, meta] : snapshots_) {
    result.push_back(meta);
  }
  return result;
}

std::size_t SnapshotEngine::activeCount() const {
  std::lock_guard<std::mutex> lock(mu_);
  std::size_t count = 0;
  for (const auto& [id, meta] : snapshots_) {
    if (meta.state == SnapshotState::Running || meta.state == SnapshotState::Pending) {
      count++;
    }
  }
  return count;
}

std::size_t SnapshotEngine::totalSnapshots() const {
  std::lock_guard<std::mutex> lock(mu_);
  return snapshots_.size();
}

std::size_t SnapshotEngine::eventsEmitted() const {
  return events_emitted_.load();
}

bool SnapshotEngine::isActive(SnapshotId id) const {
  std::lock_guard<std::mutex> lock(mu_);
  auto it = snapshots_.find(id);
  if (it == snapshots_.end()) return false;
  return it->second.state == SnapshotState::Running || it->second.state == SnapshotState::Pending;
}

void SnapshotEngine::emit(SnapshotEventKind kind, const SnapshotMeta& meta) {
  events_emitted_++;
  if (callback_) {
    SnapshotEvent event;
    event.id = meta.id;
    event.event_kind = kind;
    event.snapshot_kind = meta.kind;
    event.collector_name = meta.collector_name;
    event.items_expected = meta.items_expected;
    event.items_captured = meta.items_captured;
    event.error_message = meta.error_message;
    event.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count();
    callback_(event);
  }
}

}  // namespace monix::collectors::snapshot
