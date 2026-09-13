#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>

#include "HealthTypes.hpp"

namespace monix::collectors::health {

class CollectorHealthTracker {
public:
  CollectorHealthTracker();
  ~CollectorHealthTracker();

  CollectorHealthTracker(const CollectorHealthTracker&) = delete;
  CollectorHealthTracker& operator=(const CollectorHealthTracker&) = delete;

  void setCallback(std::function<void(const HealthEvent&)> cb);

  void recordEvent(const std::string& collector, std::int64_t latency_us = 0);
  void recordDrop(const std::string& collector);
  void recordError(const std::string& collector);
  void recordSuccess(const std::string& collector, std::int64_t latency_us = 0);

  void setQueueDepth(const std::string& collector, std::size_t depth);
  void setCpuUsage(const std::string& collector, double cpu);
  void setMemoryUsage(const std::string& collector, std::size_t bytes);
  void setObservationGapCount(const std::string& collector, std::size_t count);

  CollectorMetrics getMetrics(const std::string& collector) const;

  void transition(const std::string& collector, CollectorHealthState new_state, const std::string& reason = "");
  CollectorHealthState getState(const std::string& collector) const;

  void reset(const std::string& collector);
  void resetAll();

  std::size_t eventsEmitted() const;

private:
  void emitHealth(const std::string& collector, CollectorHealthState state, const std::string& reason);

  std::function<void(const HealthEvent&)> callback_;
  mutable std::mutex mu_;
  std::unordered_map<std::string, CollectorMetrics> metrics_;
  std::unordered_map<std::string, CollectorHealthState> states_;
  std::atomic<std::size_t> events_emitted_{0};
};

}  // namespace monix::collectors::health
