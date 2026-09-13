#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace monix::collectors::eventstore {

enum class EventSeverity : std::uint8_t {
  Debug = 0,
  Info = 1,
  Warning = 2,
  Error = 3,
  Critical = 4
};

const char* EventSeverityName(EventSeverity s);

enum class EventPriority : std::uint8_t {
  Low = 0,
  Normal = 1,
  High = 2,
  Critical = 3
};

const char* EventPriorityName(EventPriority p);

struct StoredEvent {
  std::string event_id;
  std::int64_t timestamp_ms = 0;
  std::string event_type;
  EventSeverity severity = EventSeverity::Info;
  EventPriority priority = EventPriority::Normal;
  std::string actor;
  std::string entity;
  std::string source;
  std::string correlation_id;
  std::string parent_event_id;
  std::string activity_id;
  std::string session_id;
  std::string payload;
  std::string provenance;
  std::string hash;
  std::int64_t stored_at_ms = 0;

  bool isValid() const;
  std::string summary() const;
};

struct StorageConfig {
  std::size_t max_events = 1000000;
  std::size_t rotation_size = 100000;
  std::int64_t rotation_interval_ms = 3600000;
  std::size_t write_queue_size = 10000;
  bool sync_writes = false;

  bool isValid() const;
};

struct QueryFilter {
  std::string event_type;
  std::string source;
  std::string actor;
  std::string entity;
  std::string correlation_id;
  std::string activity_id;
  std::string session_id;
  EventSeverity min_severity = EventSeverity::Debug;
  EventSeverity max_severity = EventSeverity::Critical;
  std::int64_t time_from_ms = 0;
  std::int64_t time_to_ms = 0;
  std::size_t max_results = 1000;
  std::size_t offset = 0;

  bool hasTimeRange() const;
  bool matches(const StoredEvent& event) const;
};

struct StorageMetrics {
  std::size_t total_stored = 0;
  std::size_t total_queries = 0;
  std::size_t total_rotations = 0;
  std::size_t queue_depth = 0;
  std::int64_t last_write_ms = 0;
  std::int64_t last_query_ms = 0;

  std::string summary() const;
};

using StorageCallback = std::function<void(const StoredEvent&)>;

class EventStorage {
public:
  EventStorage();
  explicit EventStorage(const StorageConfig& cfg);
  ~EventStorage();

  EventStorage(const EventStorage&) = delete;
  EventStorage& operator=(const EventStorage&) = delete;

  void setCallback(StorageCallback cb);

  void insert(const StoredEvent& event);
  void insertBatch(const std::vector<StoredEvent>& events);

  std::vector<StoredEvent> query(const QueryFilter& filter) const;
  StoredEvent getById(const std::string& event_id) const;
  std::vector<StoredEvent> byCorrelationId(const std::string& correlation_id) const;
  std::vector<StoredEvent> byActivityId(const std::string& activity_id) const;
  std::vector<StoredEvent> byTimeRange(std::int64_t from_ms, std::int64_t to_ms) const;

  std::size_t count() const;
  std::size_t count(const QueryFilter& filter) const;

  bool rotate();
  bool retain(const QueryFilter& removal_filter);
  bool compact();

  StorageMetrics metrics() const;
  StorageConfig config() const;

  void clear();
  void flush();

private:
  void processQueue();

  StorageCallback callback_;
  mutable std::mutex mu_;
  StorageConfig config_;
  std::vector<StoredEvent> events_;
  std::vector<StoredEvent> write_queue_;
  std::unordered_map<std::string, std::size_t> id_index_;
  std::unordered_map<std::string, std::vector<std::size_t>> correlation_index_;
  std::unordered_map<std::string, std::vector<std::size_t>> activity_index_;
  StorageMetrics metrics_;
  std::atomic<bool> dirty_{false};
};

}  // namespace monix::collectors::eventstore
