#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "HealthTypes.hpp"

namespace monix::collectors::health {

using GapCallback = std::function<void(const GapEvent&)>;
using HealthCallback = std::function<void(const HealthEvent&)>;

class ObservationGapDetector {
public:
  ObservationGapDetector();
  ~ObservationGapDetector();

  ObservationGapDetector(const ObservationGapDetector&) = delete;
  ObservationGapDetector& operator=(const ObservationGapDetector&) = delete;

  void setGapCallback(GapCallback cb);
  void setHealthCallback(HealthCallback cb);

  void collectorStarted(const std::string& collector, std::int64_t now_ms = 0);
  void collectorStopped(const std::string& collector, const std::string& reason = "", std::int64_t now_ms = 0);

  void heartbeat(const std::string& collector, std::int64_t now_ms = 0);

  ObservationGap lastGap(const std::string& collector) const;
  std::vector<ObservationGap> allGaps(const std::string& collector) const;
  std::size_t gapCount(const std::string& collector) const;

  CollectorHealthState state(const std::string& collector) const;
  bool isRunning(const std::string& collector) const;
  bool hasGap(const std::string& collector) const;

  std::size_t totalGaps() const;
  std::size_t eventsEmitted() const;

private:
  void emitGap(const std::string& collector, std::int64_t start_ms, std::int64_t end_ms, const std::string& reason);
  void emitHealth(const std::string& collector, CollectorHealthState state, const std::string& reason);
  static std::int64_t nowMs();

  GapCallback gap_callback_;
  HealthCallback health_callback_;
  mutable std::mutex mu_;

  struct CollectorState {
    CollectorHealthState state = CollectorHealthState::Unknown;
    std::int64_t last_started_ms = 0;
    std::int64_t last_stopped_ms = 0;
    std::vector<ObservationGap> gaps;
  };

  std::unordered_map<std::string, CollectorState> collectors_;
  std::atomic<std::size_t> total_gaps_{0};
  std::atomic<std::size_t> events_emitted_{0};
};

}  // namespace monix::collectors::health
