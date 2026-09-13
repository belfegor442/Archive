#include "CollectorHealth.hpp"

#include <chrono>

namespace monix::collectors::health {

CollectorHealthTracker::CollectorHealthTracker() {}
CollectorHealthTracker::~CollectorHealthTracker() {}

void CollectorHealthTracker::setCallback(std::function<void(const HealthEvent&)> cb) {
  std::lock_guard<std::mutex> lock(mu_);
  callback_ = std::move(cb);
}

void CollectorHealthTracker::recordEvent(const std::string& collector, std::int64_t latency_us) {
  std::lock_guard<std::mutex> lock(mu_);
  metrics_[collector].events_generated++;
  if (latency_us > 0) metrics_[collector].latency_us = latency_us;
}

void CollectorHealthTracker::recordDrop(const std::string& collector) {
  std::lock_guard<std::mutex> lock(mu_);
  metrics_[collector].events_dropped++;
}

void CollectorHealthTracker::recordError(const std::string& collector) {
  std::lock_guard<std::mutex> lock(mu_);
  metrics_[collector].errors++;
}

void CollectorHealthTracker::recordSuccess(const std::string& collector, std::int64_t latency_us) {
  std::lock_guard<std::mutex> lock(mu_);
  metrics_[collector].last_success_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::system_clock::now().time_since_epoch()).count();
  if (latency_us > 0) metrics_[collector].latency_us = latency_us;
}

void CollectorHealthTracker::setQueueDepth(const std::string& collector, std::size_t depth) {
  std::lock_guard<std::mutex> lock(mu_);
  metrics_[collector].queue_depth = depth;
}

void CollectorHealthTracker::setCpuUsage(const std::string& collector, double cpu) {
  std::lock_guard<std::mutex> lock(mu_);
  metrics_[collector].cpu_usage = cpu;
}

void CollectorHealthTracker::setMemoryUsage(const std::string& collector, std::size_t bytes) {
  std::lock_guard<std::mutex> lock(mu_);
  metrics_[collector].memory_bytes = bytes;
}

void CollectorHealthTracker::setObservationGapCount(const std::string& collector, std::size_t count) {
  std::lock_guard<std::mutex> lock(mu_);
  metrics_[collector].observation_gap_count = count;
}

CollectorMetrics CollectorHealthTracker::getMetrics(const std::string& collector) const {
  std::lock_guard<std::mutex> lock(mu_);
  auto it = metrics_.find(collector);
  if (it == metrics_.end()) return CollectorMetrics{};
  return it->second;
}

void CollectorHealthTracker::transition(const std::string& collector, CollectorHealthState new_state, const std::string& reason) {
  CollectorHealthState old_state;
  {
    std::lock_guard<std::mutex> lock(mu_);
    old_state = states_[collector];
    if (old_state == new_state) return;
    states_[collector] = new_state;
  }
  emitHealth(collector, new_state, reason);
}

CollectorHealthState CollectorHealthTracker::getState(const std::string& collector) const {
  std::lock_guard<std::mutex> lock(mu_);
  auto it = states_.find(collector);
  if (it == states_.end()) return CollectorHealthState::Unknown;
  return it->second;
}

void CollectorHealthTracker::reset(const std::string& collector) {
  std::lock_guard<std::mutex> lock(mu_);
  metrics_.erase(collector);
  states_.erase(collector);
}

void CollectorHealthTracker::resetAll() {
  std::lock_guard<std::mutex> lock(mu_);
  metrics_.clear();
  states_.clear();
}

std::size_t CollectorHealthTracker::eventsEmitted() const {
  return events_emitted_.load();
}

void CollectorHealthTracker::emitHealth(const std::string& collector, CollectorHealthState state, const std::string& reason) {
  events_emitted_++;
  if (callback_) {
    HealthEvent event;
    event.collector = collector;
    event.state = state;
    event.reason = reason;
    event.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count();
    callback_(event);
  }
}

}  // namespace monix::collectors::health
