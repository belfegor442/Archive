#include "CollectorMetrics.hpp"
#include "../events/EventTime.hpp"

namespace monix::collectors {

void CollectorMetrics::recordEventCollected() {
  std::lock_guard<std::mutex> lock(mu_);
  eventsCollected_++;
}

void CollectorMetrics::recordEventDropped() {
  std::lock_guard<std::mutex> lock(mu_);
  eventsDropped_++;
}

void CollectorMetrics::recordStart() {
  std::lock_guard<std::mutex> lock(mu_);
  startCount_++;
  lastStartTime_ = events::currentSystemTimeMs();
}

void CollectorMetrics::recordStop() {
  std::lock_guard<std::mutex> lock(mu_);
  stopCount_++;
  lastStopTime_ = events::currentSystemTimeMs();
}

void CollectorMetrics::recordError() {
  std::lock_guard<std::mutex> lock(mu_);
  errorCount_++;
}

CollectorMetricsSnapshot CollectorMetrics::snapshot() const {
  std::lock_guard<std::mutex> lock(mu_);
  CollectorMetricsSnapshot s;
  s.events_collected = eventsCollected_;
  s.events_dropped = eventsDropped_;
  s.start_count = startCount_;
  s.stop_count = stopCount_;
  s.error_count = errorCount_;
  s.last_start_time = lastStartTime_;
  s.last_stop_time = lastStopTime_;
  if (lastStartTime_ > 0 && lastStopTime_ > lastStartTime_) {
    double elapsedSec = static_cast<double>(lastStopTime_ - lastStartTime_) / 1000.0;
    if (elapsedSec > 0) {
      s.collection_rate = static_cast<double>(eventsCollected_) / elapsedSec;
    }
  }
  return s;
}

void CollectorMetrics::reset() {
  std::lock_guard<std::mutex> lock(mu_);
  eventsCollected_ = 0;
  eventsDropped_ = 0;
  startCount_ = 0;
  stopCount_ = 0;
  errorCount_ = 0;
  lastStartTime_ = 0;
  lastStopTime_ = 0;
}

}  // namespace monix::collectors
