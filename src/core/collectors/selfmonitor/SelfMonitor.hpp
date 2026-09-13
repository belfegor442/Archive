#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace monix::collectors::selfmonitor {

enum class MetricType : std::uint8_t {
  Counter,
  Gauge,
  Histogram,
  Rate
};

const char* MetricTypeName(MetricType t);

enum class HealthStatus : std::uint8_t {
  Healthy,
  Degraded,
  Unhealthy,
  Critical
};

const char* HealthStatusName(HealthStatus s);

struct MetricValue {
  double value = 0.0;
  std::int64_t timestamp_ms = 0;
  std::size_t sample_count = 0;
};

struct AggregatedMetric {
  std::string name;
  MetricType type = MetricType::Counter;
  double sum = 0.0;
  double min_val = 0.0;
  double max_val = 0.0;
  double avg = 0.0;
  std::size_t count = 0;
  std::int64_t last_updated_ms = 0;

  void update(double value, std::int64_t now_ms);
  std::string summary() const;
};

struct SystemHealth {
  HealthStatus overall = HealthStatus::Healthy;
  HealthStatus eventbus = HealthStatus::Healthy;
  HealthStatus collectors = HealthStatus::Healthy;
  HealthStatus storage = HealthStatus::Healthy;
  HealthStatus validation = HealthStatus::Healthy;

  std::size_t unhealthy_components = 0;
  std::string worst_component;

  std::string summary() const;
};

struct SelfMonitorMetrics {
  double eventbus_latency_ms = 0.0;
  std::size_t queue_depth = 0;
  double events_per_sec = 0.0;
  std::size_t dropped_events = 0;
  std::size_t validation_failures = 0;
  std::size_t quarantine_size = 0;
  double storage_latency_ms = 0.0;
  std::size_t storage_failures = 0;
  std::size_t collector_healthy = 0;
  std::size_t collector_degraded = 0;
  std::size_t collector_unhealthy = 0;

  std::string summary() const;
};

class SelfMonitor {
public:
  SelfMonitor();
  ~SelfMonitor();

  SelfMonitor(const SelfMonitor&) = delete;
  SelfMonitor& operator=(const SelfMonitor&) = delete;

  void recordMetric(const std::string& name, double value, MetricType type = MetricType::Gauge);
  void incrementCounter(const std::string& name, std::size_t amount = 1);

  AggregatedMetric getMetric(const std::string& name) const;
  std::vector<AggregatedMetric> allMetrics() const;

  void setQueueDepth(std::size_t depth);
  void setEventBusLatency(double ms);
  void setStorageLatency(double ms);
  void setEventsPerSec(double eps);
  void addDroppedEvents(std::size_t count);
  void addValidationFailure(std::size_t count);
  void addQuarantineEntry(std::size_t count);
  void removeQuarantineEntry(std::size_t count);
  void addStorageFailure(std::size_t count);
  void setCollectorStatus(const std::string& name, HealthStatus status);

  SelfMonitorMetrics snapshot() const;
  SystemHealth health() const;

  void reset();
  std::size_t totalRecordings() const;

private:
  void updateHealthFromMetrics();

  mutable std::mutex mu_;
  std::unordered_map<std::string, AggregatedMetric> metrics_;
  std::unordered_map<std::string, HealthStatus> collector_statuses_;
  std::atomic<std::size_t> total_recordings_{0};

  std::size_t queue_depth_ = 0;
  double eventbus_latency_ms_ = 0.0;
  double storage_latency_ms_ = 0.0;
  double events_per_sec_ = 0.0;
  std::size_t dropped_events_ = 0;
  std::size_t validation_failures_ = 0;
  std::size_t quarantine_size_ = 0;
  std::size_t storage_failures_ = 0;
};

}  // namespace monix::collectors::selfmonitor
