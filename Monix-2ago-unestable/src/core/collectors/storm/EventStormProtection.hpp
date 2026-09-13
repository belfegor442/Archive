#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace monix::collectors::storm {

enum class StormPriority : std::uint8_t {
  Critical = 0,
  High = 1,
  Normal = 2,
  Low = 3,
  Background = 4
};

const char* StormPriorityName(StormPriority p);

enum class LossReason : std::uint8_t {
  QueueLimit,
  RateLimit,
  Coalescing,
  Deduplication,
  Aggregation,
  Backpressure
};

const char* LossReasonName(LossReason r);

struct StormConfig {
  std::size_t queue_limit = 10000;
  std::size_t rate_limit_per_second = 5000;
  std::size_t coalesce_window_ms = 100;
  std::size_t dedup_window_ms = 500;
  std::size_t aggregate_window_ms = 200;
  std::size_t aggregate_max_batch = 100;
  StormPriority min_priority = StormPriority::Normal;

  bool isValid() const;
};

struct StormEvent {
  std::string event_id;
  std::string collector;
  std::string event_type;
  StormPriority priority = StormPriority::Normal;
  std::int64_t timestamp_ms = 0;
  double numeric_value = 0.0;

  bool isValid() const;
};

struct LossEvent {
  std::string collector;
  std::size_t events_dropped = 0;
  std::int64_t window_start_ms = 0;
  std::int64_t window_end_ms = 0;
  LossReason reason = LossReason::QueueLimit;
  std::int64_t timestamp_ms = 0;

  bool isValid() const;
  std::string summary() const;
  std::string action() const;
};

struct StormResult {
  bool accepted = true;
  LossReason loss_reason = LossReason::QueueLimit;
  std::string reason;

  bool wasAccepted() const;
  std::string summary() const;
};

using StormCallback = std::function<void(const LossEvent&)>;
using EventCallback = std::function<void(const StormEvent&)>;

class StormProtector {
public:
  StormProtector();
  explicit StormProtector(const StormConfig& cfg);
  ~StormProtector();

  StormProtector(const StormProtector&) = delete;
  StormProtector& operator=(const StormProtector&) = delete;

  void setLossCallback(StormCallback cb);
  void setEventCallback(EventCallback cb);

  void setConfig(const StormConfig& cfg);
  StormConfig getConfig() const;

  StormResult ingest(const StormEvent& event);

  std::size_t totalIngested() const;
  std::size_t totalAccepted() const;
  std::size_t totalDropped() const;
  std::size_t totalCoalesced() const;

  void flush();
  void reset();

private:
  StormResult evaluateQueue(const StormEvent& event);
  StormResult evaluateRate(const StormEvent& event);
  StormResult evaluateDedup(const StormEvent& event);
  StormResult evaluatePriority(const StormEvent& event);

  void emitLoss(const std::string& collector, std::size_t count, LossReason reason,
    std::int64_t window_start, std::int64_t window_end);
  void emitEvent(const StormEvent& event);

  StormCallback loss_callback_;
  EventCallback event_callback_;
  mutable std::mutex mu_;
  StormConfig config_;
  std::atomic<std::size_t> total_ingested_{0};
  std::atomic<std::size_t> total_accepted_{0};
  std::atomic<std::size_t> total_dropped_{0};
  std::atomic<std::size_t> total_coalesced_{0};

  struct CollectorState {
    std::vector<std::pair<std::string, std::int64_t>> recent_ids;
    std::size_t rate_count = 0;
    std::int64_t rate_window_start = 0;
    std::size_t queue_size = 0;
    std::size_t pending_loss = 0;
    std::int64_t loss_window_start = 0;
    std::int64_t loss_window_end = 0;
  };

  std::unordered_map<std::string, CollectorState> states_;
};

}  // namespace monix::collectors::storm
