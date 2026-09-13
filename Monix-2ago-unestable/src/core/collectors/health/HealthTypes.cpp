#include "HealthTypes.hpp"

namespace monix::collectors::health {

const char* GapEventKindName(GapEventKind k) {
  switch (k) {
    case GapEventKind::GapDetected: return "GapDetected";
    case GapEventKind::Recovered:   return "Recovered";
  }
  return "Unknown";
}

std::string GapEventKindAction(GapEventKind k) {
  switch (k) {
    case GapEventKind::GapDetected: return "collector.observation_gap";
    case GapEventKind::Recovered:   return "collector.recovered";
  }
  return "unknown";
}

const char* CollectorHealthStateName(CollectorHealthState s) {
  switch (s) {
    case CollectorHealthState::Unknown:   return "Unknown";
    case CollectorHealthState::Started:   return "Started";
    case CollectorHealthState::Running:   return "Running";
    case CollectorHealthState::Degraded:  return "Degraded";
    case CollectorHealthState::Failed:    return "Failed";
    case CollectorHealthState::Stopped:   return "Stopped";
    case CollectorHealthState::Recovered: return "Recovered";
  }
  return "Unknown";
}

bool ObservationGap::isValid() const {
  return !collector.empty();
}

bool ObservationGap::isActive() const {
  return ended_at_ms == 0;
}

std::string ObservationGap::summary() const {
  std::string result = collector + " gap " + std::to_string(duration_ms) + "ms";
  result.reserve(256);
  if (!reason.empty()) result += " (" + reason + ")";
  return result;
}

bool GapEvent::isValid() const {
  return !collector.empty();
}

std::string GapEvent::summary() const {
  std::string result = std::string(GapEventKindName(kind)) + " " + collector;
  result.reserve(256);
  result += " " + std::to_string(duration_ms) + "ms";
  if (!reason.empty()) result += " (" + reason + ")";
  return result;
}

bool HealthEvent::isValid() const {
  return !collector.empty();
}

std::string HealthEvent::action() const {
  switch (state) {
    case CollectorHealthState::Started:   return "collector.started";
    case CollectorHealthState::Stopped:   return "collector.stopped";
    case CollectorHealthState::Degraded:  return "collector.degraded";
    case CollectorHealthState::Failed:    return "collector.failed";
    case CollectorHealthState::Recovered: return "collector.recovered";
    default: return "unknown";
  }
}

std::string HealthEvent::summary() const {
  return collector + " " + std::string(CollectorHealthStateName(state));
}

bool CollectorMetrics::isValid() const {
  return true;
}

double CollectorMetrics::errorRate() const {
  if (events_generated == 0) return 0.0;
  return static_cast<double>(errors) / static_cast<double>(events_generated);
}

double CollectorMetrics::dropRate() const {
  if (events_generated == 0) return 0.0;
  return static_cast<double>(events_dropped) / static_cast<double>(events_generated);
}

std::string CollectorMetrics::summary() const {
  std::string result = "gen=" + std::to_string(events_generated);
  result.reserve(256);
  result += " drop=" + std::to_string(events_dropped);
  result += " err=" + std::to_string(errors);
  result += " lat=" + std::to_string(latency_us) + "us";
  result += " q=" + std::to_string(queue_depth);
  result += " gaps=" + std::to_string(observation_gap_count);
  return result;
}

}  // namespace monix::collectors::health
