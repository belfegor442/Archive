#pragma once

#include "CollectorStatus.hpp"
#include "../events/EventTime.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <mutex>

namespace monix::collectors {

struct CollectorMetricsSnapshot {
  std::uint64_t events_collected = 0;
  std::uint64_t events_dropped = 0;
  std::uint64_t start_count = 0;
  std::uint64_t stop_count = 0;
  std::uint64_t error_count = 0;
  events::Timestamp last_start_time = 0;
  events::Timestamp last_stop_time = 0;
  double collection_rate = 0.0;
};

class CollectorMetrics {
public:
  void recordEventCollected();
  void recordEventDropped();
  void recordStart();
  void recordStop();
  void recordError();

  CollectorMetricsSnapshot snapshot() const;
  void reset();

private:
  mutable std::mutex mu_;
  std::uint64_t eventsCollected_ = 0;
  std::uint64_t eventsDropped_ = 0;
  std::uint64_t startCount_ = 0;
  std::uint64_t stopCount_ = 0;
  std::uint64_t errorCount_ = 0;
  events::Timestamp lastStartTime_ = 0;
  events::Timestamp lastStopTime_ = 0;
};

}  // namespace monix::collectors
