#include "SelfMonitor.hpp"

#include <algorithm>
#include <chrono>
#include <sstream>

namespace monix::collectors::selfmonitor {

const char* MetricTypeName(MetricType t) {
  switch (t) {
    case MetricType::Counter:   return "Counter";
    case MetricType::Gauge:     return "Gauge";
    case MetricType::Histogram: return "Histogram";
    case MetricType::Rate:      return "Rate";
  }
  return "Unknown";
}

const char* HealthStatusName(HealthStatus s) {
  switch (s) {
    case HealthStatus::Healthy:   return "Healthy";
    case HealthStatus::Degraded:  return "Degraded";
    case HealthStatus::Unhealthy: return "Unhealthy";
    case HealthStatus::Critical:  return "Critical";
  }
  return "Unknown";
}

void AggregatedMetric::update(double value, std::int64_t now_ms) {
  if (count == 0) {
    min_val = value;
    max_val = value;
  } else {
    if (value < min_val) min_val = value;
    if (value > max_val) max_val = value;
  }
  sum += value;
  count++;
  avg = sum / static_cast<double>(count);
  last_updated_ms = now_ms;
}

std::string AggregatedMetric::summary() const {
  return name + " type=" + std::string(MetricTypeName(type)) +
    " avg=" + std::to_string(avg) +
    " min=" + std::to_string(min_val) +
    " max=" + std::to_string(max_val) +
    " count=" + std::to_string(count);
}

std::string SystemHealth::summary() const {
  return "overall=" + std::string(HealthStatusName(overall)) +
    " unhealthy=" + std::to_string(unhealthy_components);
}

std::string SelfMonitorMetrics::summary() const {
  return "eb_lat=" + std::to_string(eventbus_latency_ms) +
    " qd=" + std::to_string(queue_depth) +
    " eps=" + std::to_string(events_per_sec) +
    " dropped=" + std::to_string(dropped_events) +
    " vf=" + std::to_string(validation_failures) +
    " qs=" + std::to_string(quarantine_size) +
    " st_lat=" + std::to_string(storage_latency_ms) +
    " st_fail=" + std::to_string(storage_failures);
}

SelfMonitor::SelfMonitor() {}
SelfMonitor::~SelfMonitor() {}

void SelfMonitor::recordMetric(const std::string& name, double value, MetricType type) {
  std::lock_guard<std::mutex> lock(mu_);
  auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::system_clock::now().time_since_epoch()).count();

  auto it = metrics_.find(name);
  if (it != metrics_.end()) {
    it->second.update(value, now_ms);
    it->second.type = type;
  } else {
    AggregatedMetric m;
    m.name = name;
    m.type = type;
    m.update(value, now_ms);
    metrics_[name] = m;
  }
  total_recordings_++;
}

void SelfMonitor::incrementCounter(const std::string& name, std::size_t amount) {
  std::lock_guard<std::mutex> lock(mu_);
  auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::system_clock::now().time_since_epoch()).count();

  auto it = metrics_.find(name);
  if (it != metrics_.end()) {
    it->second.update(static_cast<double>(amount), now_ms);
  } else {
    AggregatedMetric m;
    m.name = name;
    m.type = MetricType::Counter;
    m.update(static_cast<double>(amount), now_ms);
    metrics_[name] = m;
  }
  total_recordings_++;
}

AggregatedMetric SelfMonitor::getMetric(const std::string& name) const {
  std::lock_guard<std::mutex> lock(mu_);
  auto it = metrics_.find(name);
  if (it != metrics_.end()) return it->second;
  return AggregatedMetric{};
}

std::vector<AggregatedMetric> SelfMonitor::allMetrics() const {
  std::lock_guard<std::mutex> lock(mu_);
  std::vector<AggregatedMetric> result;
  for (const auto& [name, metric] : metrics_) {
    result.push_back(metric);
  }
  return result;
}

void SelfMonitor::setQueueDepth(std::size_t depth) {
  std::lock_guard<std::mutex> lock(mu_);
  queue_depth_ = depth;
  auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::system_clock::now().time_since_epoch()).count();
  auto it = metrics_.find("queue_depth");
  if (it != metrics_.end()) {
    it->second.update(static_cast<double>(depth), now_ms);
  } else {
    AggregatedMetric m;
    m.name = "queue_depth";
    m.type = MetricType::Gauge;
    m.update(static_cast<double>(depth), now_ms);
    metrics_["queue_depth"] = m;
  }
}

void SelfMonitor::setEventBusLatency(double ms) {
  std::lock_guard<std::mutex> lock(mu_);
  eventbus_latency_ms_ = ms;
  auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::system_clock::now().time_since_epoch()).count();
  metrics_["eventbus_latency"].update(ms, now_ms);
}

void SelfMonitor::setStorageLatency(double ms) {
  std::lock_guard<std::mutex> lock(mu_);
  storage_latency_ms_ = ms;
  auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::system_clock::now().time_since_epoch()).count();
  metrics_["storage_latency"].update(ms, now_ms);
}

void SelfMonitor::setEventsPerSec(double eps) {
  std::lock_guard<std::mutex> lock(mu_);
  events_per_sec_ = eps;
  auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::system_clock::now().time_since_epoch()).count();
  metrics_["events_per_sec"].update(eps, now_ms);
}

void SelfMonitor::addDroppedEvents(std::size_t count) {
  std::lock_guard<std::mutex> lock(mu_);
  dropped_events_ += count;
}

void SelfMonitor::addValidationFailure(std::size_t count) {
  std::lock_guard<std::mutex> lock(mu_);
  validation_failures_ += count;
}

void SelfMonitor::addQuarantineEntry(std::size_t count) {
  std::lock_guard<std::mutex> lock(mu_);
  quarantine_size_ += count;
}

void SelfMonitor::removeQuarantineEntry(std::size_t count) {
  std::lock_guard<std::mutex> lock(mu_);
  if (count > quarantine_size_) quarantine_size_ = 0;
  else quarantine_size_ -= count;
}

void SelfMonitor::addStorageFailure(std::size_t count) {
  std::lock_guard<std::mutex> lock(mu_);
  storage_failures_ += count;
}

void SelfMonitor::setCollectorStatus(const std::string& name, HealthStatus status) {
  std::lock_guard<std::mutex> lock(mu_);
  collector_statuses_[name] = status;
}

SelfMonitorMetrics SelfMonitor::snapshot() const {
  std::lock_guard<std::mutex> lock(mu_);
  SelfMonitorMetrics m;
  m.eventbus_latency_ms = eventbus_latency_ms_;
  m.queue_depth = queue_depth_;
  m.events_per_sec = events_per_sec_;
  m.dropped_events = dropped_events_;
  m.validation_failures = validation_failures_;
  m.quarantine_size = quarantine_size_;
  m.storage_latency_ms = storage_latency_ms_;
  m.storage_failures = storage_failures_;

  for (const auto& [name, status] : collector_statuses_) {
    if (status == HealthStatus::Healthy) m.collector_healthy++;
    else if (status == HealthStatus::Degraded) m.collector_degraded++;
    else m.collector_unhealthy++;
  }
  return m;
}

SystemHealth SelfMonitor::health() const {
  std::lock_guard<std::mutex> lock(mu_);
  SystemHealth h;

  std::size_t unhealthy = 0;
  HealthStatus worst = HealthStatus::Healthy;
  std::string worst_name;

  for (const auto& [name, status] : collector_statuses_) {
    if (status > worst) {
      worst = status;
      worst_name = name;
    }
    if (status >= HealthStatus::Unhealthy) unhealthy++;
  }

  h.overall = worst;
  h.unhealthy_components = unhealthy;
  h.worst_component = worst_name;

  if (storage_failures_ > 100) h.storage = HealthStatus::Unhealthy;
  else if (storage_failures_ > 10) h.storage = HealthStatus::Degraded;

  if (validation_failures_ > 1000) h.validation = HealthStatus::Unhealthy;
  else if (validation_failures_ > 100) h.validation = HealthStatus::Degraded;

  if (dropped_events_ > 10000) h.eventbus = HealthStatus::Unhealthy;
  else if (dropped_events_ > 100) h.eventbus = HealthStatus::Degraded;

  return h;
}

void SelfMonitor::reset() {
  std::lock_guard<std::mutex> lock(mu_);
  metrics_.clear();
  collector_statuses_.clear();
  queue_depth_ = 0;
  eventbus_latency_ms_ = 0;
  storage_latency_ms_ = 0;
  events_per_sec_ = 0;
  dropped_events_ = 0;
  validation_failures_ = 0;
  quarantine_size_ = 0;
  storage_failures_ = 0;
  total_recordings_ = 0;
}

std::size_t SelfMonitor::totalRecordings() const {
  return total_recordings_;
}

}  // namespace monix::collectors::selfmonitor
