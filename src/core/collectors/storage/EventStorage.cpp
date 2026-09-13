#include "EventStorage.hpp"

#include <algorithm>
#include <chrono>

namespace monix::collectors::eventstore {

const char* EventSeverityName(EventSeverity s) {
  switch (s) {
    case EventSeverity::Debug:    return "Debug";
    case EventSeverity::Info:     return "Info";
    case EventSeverity::Warning:  return "Warning";
    case EventSeverity::Error:    return "Error";
    case EventSeverity::Critical: return "Critical";
  }
  return "Unknown";
}

const char* EventPriorityName(EventPriority p) {
  switch (p) {
    case EventPriority::Low:      return "Low";
    case EventPriority::Normal:   return "Normal";
    case EventPriority::High:     return "High";
    case EventPriority::Critical: return "Critical";
  }
  return "Unknown";
}

bool StoredEvent::isValid() const {
  return !event_id.empty() && !event_type.empty();
}

std::string StoredEvent::summary() const {
  std::string result = event_id + " [" + event_type + "]";
  result += " " + std::string(EventSeverityName(severity));
  if (!source.empty()) result += " src=" + source;
  if (!actor.empty()) result += " actor=" + actor;
  return result;
}

bool StorageConfig::isValid() const {
  if (max_events == 0) return false;
  if (rotation_size == 0) return false;
  if (rotation_size >= max_events) return false;
  return true;
}

bool QueryFilter::hasTimeRange() const {
  return time_from_ms > 0 || time_to_ms > 0;
}

bool QueryFilter::matches(const StoredEvent& event) const {
  if (!event_type.empty() && event.event_type != event_type) return false;
  if (!source.empty() && event.source != source) return false;
  if (!actor.empty() && event.actor != actor) return false;
  if (!entity.empty() && event.entity != entity) return false;
  if (!correlation_id.empty() && event.correlation_id != correlation_id) return false;
  if (!activity_id.empty() && event.activity_id != activity_id) return false;
  if (!session_id.empty() && event.session_id != session_id) return false;

  if (static_cast<std::uint8_t>(event.severity) < static_cast<std::uint8_t>(min_severity)) return false;
  if (static_cast<std::uint8_t>(event.severity) > static_cast<std::uint8_t>(max_severity)) return false;

  if (time_from_ms > 0 && event.timestamp_ms < time_from_ms) return false;
  if (time_to_ms > 0 && event.timestamp_ms > time_to_ms) return false;

  return true;
}

std::string StorageMetrics::summary() const {
  return "stored=" + std::to_string(total_stored) +
    " queries=" + std::to_string(total_queries) +
    " rotations=" + std::to_string(total_rotations) +
    " queue=" + std::to_string(queue_depth);
}

EventStorage::EventStorage() {}
EventStorage::EventStorage(const StorageConfig& cfg) : config_(cfg) {}
EventStorage::~EventStorage() {}

void EventStorage::setCallback(StorageCallback cb) {
  std::lock_guard<std::mutex> lock(mu_);
  callback_ = std::move(cb);
}

void EventStorage::insert(const StoredEvent& event) {
  StoredEvent stored = event;
  StorageCallback callback;
  {
    std::lock_guard<std::mutex> lock(mu_);
    if (events_.size() >= config_.max_events) rotate();
    stored.stored_at_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count();
    std::size_t idx = events_.size();
    events_.push_back(stored);
    id_index_[stored.event_id] = idx;
    if (!stored.correlation_id.empty()) correlation_index_[stored.correlation_id].push_back(idx);
    if (!stored.activity_id.empty()) activity_index_[stored.activity_id].push_back(idx);
    metrics_.total_stored++;
    metrics_.last_write_ms = stored.stored_at_ms;
    dirty_ = true;
    callback = callback_;
  }
  if (callback) callback(stored);
}

void EventStorage::insertBatch(const std::vector<StoredEvent>& events) {
  auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::system_clock::now().time_since_epoch()).count();
  std::vector<StoredEvent> storedEvents;
  StorageCallback callback;
  {
    std::lock_guard<std::mutex> lock(mu_);
    callback = callback_;
    storedEvents.reserve(events.size());

    for (const auto& event : events) {
      if (events_.size() >= config_.max_events) rotate();

      StoredEvent stored = event;
      stored.stored_at_ms = now_ms;

      std::size_t idx = events_.size();
      events_.push_back(stored);
      id_index_[stored.event_id] = idx;

      if (!stored.correlation_id.empty()) correlation_index_[stored.correlation_id].push_back(idx);
      if (!stored.activity_id.empty()) activity_index_[stored.activity_id].push_back(idx);

      metrics_.total_stored++;
      metrics_.last_write_ms = now_ms;
      storedEvents.push_back(std::move(stored));
    }
    dirty_ = true;
  }
  if (callback) {
    for (const auto& stored : storedEvents) callback(stored);
  }
}

std::vector<StoredEvent> EventStorage::query(const QueryFilter& filter) const {
  std::lock_guard<std::mutex> lock(mu_);
  const_cast<StorageMetrics&>(metrics_).total_queries++;
  const_cast<StorageMetrics&>(metrics_).last_query_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::system_clock::now().time_since_epoch()).count();

  std::vector<StoredEvent> result;
  std::size_t skipped = 0;

  for (const auto& event : events_) {
    if (!filter.matches(event)) continue;
    if (skipped < filter.offset) {
      skipped++;
      continue;
    }
    result.push_back(event);
    if (result.size() >= filter.max_results) break;
  }

  return result;
}

StoredEvent EventStorage::getById(const std::string& event_id) const {
  std::lock_guard<std::mutex> lock(mu_);
  auto it = id_index_.find(event_id);
  if (it == id_index_.end()) return StoredEvent{};
  return events_[it->second];
}

std::vector<StoredEvent> EventStorage::byCorrelationId(const std::string& correlation_id) const {
  std::lock_guard<std::mutex> lock(mu_);
  std::vector<StoredEvent> result;
  auto it = correlation_index_.find(correlation_id);
  if (it == correlation_index_.end()) return result;
  for (auto idx : it->second) {
    if (idx < events_.size()) result.push_back(events_[idx]);
  }
  return result;
}

std::vector<StoredEvent> EventStorage::byActivityId(const std::string& activity_id) const {
  std::lock_guard<std::mutex> lock(mu_);
  std::vector<StoredEvent> result;
  auto it = activity_index_.find(activity_id);
  if (it == activity_index_.end()) return result;
  for (auto idx : it->second) {
    if (idx < events_.size()) result.push_back(events_[idx]);
  }
  return result;
}

std::vector<StoredEvent> EventStorage::byTimeRange(std::int64_t from_ms, std::int64_t to_ms) const {
  QueryFilter filter;
  filter.time_from_ms = from_ms;
  filter.time_to_ms = to_ms;
  return query(filter);
}

std::size_t EventStorage::count() const {
  std::lock_guard<std::mutex> lock(mu_);
  return events_.size();
}

std::size_t EventStorage::count(const QueryFilter& filter) const {
  std::lock_guard<std::mutex> lock(mu_);
  std::size_t c = 0;
  for (const auto& event : events_) {
    if (filter.matches(event)) c++;
  }
  return c;
}

bool EventStorage::rotate() {
  if (events_.size() <= config_.rotation_size) return false;

  std::size_t remove_count = events_.size() - config_.rotation_size;
  events_.erase(events_.begin(), events_.begin() + remove_count);

  id_index_.clear();
  correlation_index_.clear();
  activity_index_.clear();

  for (std::size_t i = 0; i < events_.size(); i++) {
    id_index_[events_[i].event_id] = i;
    if (!events_[i].correlation_id.empty()) {
      correlation_index_[events_[i].correlation_id].push_back(i);
    }
    if (!events_[i].activity_id.empty()) {
      activity_index_[events_[i].activity_id].push_back(i);
    }
  }

  metrics_.total_rotations++;
  return true;
}

bool EventStorage::retain(const QueryFilter& removal_filter) {
  std::lock_guard<std::mutex> lock(mu_);
  std::size_t before = events_.size();
  events_.erase(
    std::remove_if(events_.begin(), events_.end(),
      [&](const StoredEvent& e) { return removal_filter.matches(e); }),
    events_.end());

  if (events_.size() < before) {
    id_index_.clear();
    correlation_index_.clear();
    activity_index_.clear();
    for (std::size_t i = 0; i < events_.size(); i++) {
      id_index_[events_[i].event_id] = i;
      if (!events_[i].correlation_id.empty()) {
        correlation_index_[events_[i].correlation_id].push_back(i);
      }
      if (!events_[i].activity_id.empty()) {
        activity_index_[events_[i].activity_id].push_back(i);
      }
    }
    return true;
  }
  return false;
}

bool EventStorage::compact() {
  std::lock_guard<std::mutex> lock(mu_);
  if (events_.empty()) return false;

  auto max_it = std::max_element(events_.begin(), events_.end(),
    [](const StoredEvent& a, const StoredEvent& b) { return a.stored_at_ms < b.stored_at_ms; });
  std::int64_t max_stored = max_it->stored_at_ms;

  auto cutoff = max_stored - static_cast<std::int64_t>(config_.rotation_interval_ms);
  std::size_t before = events_.size();
  events_.erase(
    std::remove_if(events_.begin(), events_.end(),
      [cutoff](const StoredEvent& e) { return e.stored_at_ms < cutoff; }),
    events_.end());

  if (events_.size() < before) {
    id_index_.clear();
    correlation_index_.clear();
    activity_index_.clear();
    for (std::size_t i = 0; i < events_.size(); i++) {
      id_index_[events_[i].event_id] = i;
      if (!events_[i].correlation_id.empty()) {
        correlation_index_[events_[i].correlation_id].push_back(i);
      }
      if (!events_[i].activity_id.empty()) {
        activity_index_[events_[i].activity_id].push_back(i);
      }
    }
    return true;
  }
  return false;
}

StorageMetrics EventStorage::metrics() const {
  std::lock_guard<std::mutex> lock(mu_);
  StorageMetrics m = metrics_;
  m.queue_depth = write_queue_.size();
  return m;
}

StorageConfig EventStorage::config() const {
  std::lock_guard<std::mutex> lock(mu_);
  return config_;
}

void EventStorage::clear() {
  std::lock_guard<std::mutex> lock(mu_);
  events_.clear();
  id_index_.clear();
  correlation_index_.clear();
  activity_index_.clear();
  write_queue_.clear();
}

void EventStorage::flush() {
  std::lock_guard<std::mutex> lock(mu_);
  processQueue();
}

void EventStorage::processQueue() {
  for (const auto& event : write_queue_) {
    if (events_.size() >= config_.max_events) {
      rotate();
    }
    std::size_t idx = events_.size();
    events_.push_back(event);
    id_index_[event.event_id] = idx;
    if (!event.correlation_id.empty()) {
      correlation_index_[event.correlation_id].push_back(idx);
    }
    if (!event.activity_id.empty()) {
      activity_index_[event.activity_id].push_back(idx);
    }
  }
  write_queue_.clear();
}

}  // namespace monix::collectors::eventstore
