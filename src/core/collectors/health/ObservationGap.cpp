#include "ObservationGap.hpp"

#include <chrono>

namespace monix::collectors::health {

ObservationGapDetector::ObservationGapDetector() {}
ObservationGapDetector::~ObservationGapDetector() {}

void ObservationGapDetector::setGapCallback(GapCallback cb) {
  std::lock_guard<std::mutex> lock(mu_);
  gap_callback_ = std::move(cb);
}

void ObservationGapDetector::setHealthCallback(HealthCallback cb) {
  std::lock_guard<std::mutex> lock(mu_);
  health_callback_ = std::move(cb);
}

std::int64_t ObservationGapDetector::nowMs() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::system_clock::now().time_since_epoch()).count();
}

void ObservationGapDetector::collectorStarted(const std::string& collector, std::int64_t now_ms) {
  if (now_ms == 0) now_ms = nowMs();

  std::string reason;
  {
    std::lock_guard<std::mutex> lock(mu_);
    auto& cs = collectors_[collector];
    if (cs.state == CollectorHealthState::Running) return;

    if (cs.state == CollectorHealthState::Failed) {
      reason = "recovery";
      cs.state = CollectorHealthState::Recovered;
    } else {
      reason = "startup";
      cs.state = CollectorHealthState::Running;
    }
    cs.last_started_ms = now_ms;

    if (cs.last_stopped_ms > 0) {
      ObservationGap gap;
      gap.collector = collector;
      gap.started_at_ms = cs.last_stopped_ms;
      gap.ended_at_ms = now_ms;
      gap.duration_ms = now_ms - cs.last_stopped_ms;
      gap.reason = reason.empty() ? "collector_restart" : reason;
      cs.gaps.push_back(gap);
      total_gaps_++;
      emitGap(collector, gap.started_at_ms, gap.ended_at_ms, gap.reason);
      cs.state = CollectorHealthState::Running;
    }

    reason = cs.state == CollectorHealthState::Recovered ? "recovery" : "startup";
  }

  emitHealth(collector, CollectorHealthState::Running, reason);
}

void ObservationGapDetector::collectorStopped(const std::string& collector, const std::string& reason, std::int64_t now_ms) {
  if (now_ms == 0) now_ms = nowMs();

  {
    std::lock_guard<std::mutex> lock(mu_);
    auto& cs = collectors_[collector];
    if (cs.state == CollectorHealthState::Stopped || cs.state == CollectorHealthState::Unknown) return;
    cs.state = CollectorHealthState::Stopped;
    cs.last_stopped_ms = now_ms;
  }

  emitHealth(collector, CollectorHealthState::Stopped, reason);
}

void ObservationGapDetector::heartbeat(const std::string& collector, std::int64_t now_ms) {
  if (now_ms == 0) now_ms = nowMs();
  std::lock_guard<std::mutex> lock(mu_);
  auto& cs = collectors_[collector];
  if (cs.state == CollectorHealthState::Running) {
    cs.last_started_ms = now_ms;
  }
}

ObservationGap ObservationGapDetector::lastGap(const std::string& collector) const {
  std::lock_guard<std::mutex> lock(mu_);
  auto it = collectors_.find(collector);
  if (it == collectors_.end() || it->second.gaps.empty()) return ObservationGap{};
  return it->second.gaps.back();
}

std::vector<ObservationGap> ObservationGapDetector::allGaps(const std::string& collector) const {
  std::lock_guard<std::mutex> lock(mu_);
  auto it = collectors_.find(collector);
  if (it == collectors_.end()) return {};
  return it->second.gaps;
}

std::size_t ObservationGapDetector::gapCount(const std::string& collector) const {
  std::lock_guard<std::mutex> lock(mu_);
  auto it = collectors_.find(collector);
  if (it == collectors_.end()) return 0;
  return it->second.gaps.size();
}

CollectorHealthState ObservationGapDetector::state(const std::string& collector) const {
  std::lock_guard<std::mutex> lock(mu_);
  auto it = collectors_.find(collector);
  if (it == collectors_.end()) return CollectorHealthState::Unknown;
  return it->second.state;
}

bool ObservationGapDetector::isRunning(const std::string& collector) const {
  return state(collector) == CollectorHealthState::Running;
}

bool ObservationGapDetector::hasGap(const std::string& collector) const {
  return gapCount(collector) > 0;
}

std::size_t ObservationGapDetector::totalGaps() const {
  return total_gaps_.load();
}

std::size_t ObservationGapDetector::eventsEmitted() const {
  return events_emitted_.load();
}

void ObservationGapDetector::emitGap(const std::string& collector, std::int64_t start_ms, std::int64_t end_ms, const std::string& reason) {
  events_emitted_++;
  if (gap_callback_) {
    GapEvent event;
    event.kind = GapEventKind::GapDetected;
    event.collector = collector;
    event.started_at_ms = start_ms;
    event.ended_at_ms = end_ms;
    event.duration_ms = end_ms - start_ms;
    event.reason = reason;
    event.timestamp_ms = nowMs();
    gap_callback_(event);
  }
}

void ObservationGapDetector::emitHealth(const std::string& collector, CollectorHealthState state, const std::string& reason) {
  events_emitted_++;
  if (health_callback_) {
    HealthEvent event;
    event.collector = collector;
    event.state = state;
    event.reason = reason;
    event.timestamp_ms = nowMs();
    health_callback_(event);
  }
}

}  // namespace monix::collectors::health
