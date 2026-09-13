#include "NoiseControl.hpp"

#include <algorithm>
#include <chrono>

namespace monix::collectors::noise {

const char* NoiseMechanismName(NoiseMechanism m) {
  switch (m) {
    case NoiseMechanism::None:           return "None";
    case NoiseMechanism::Deduplication: return "Deduplication";
    case NoiseMechanism::Coalescing:     return "Coalescing";
    case NoiseMechanism::RateLimiting:   return "RateLimiting";
    case NoiseMechanism::Threshold:      return "Threshold";
    case NoiseMechanism::Aggregation:    return "Aggregation";
    case NoiseMechanism::Backpressure:   return "Backpressure";
    case NoiseMechanism::Priority:       return "Priority";
    case NoiseMechanism::Quarantine:     return "Quarantine";
  }
  return "Unknown";
}

const char* NoiseVerdictName(NoiseVerdict v) {
  switch (v) {
    case NoiseVerdict::Pass:          return "Pass";
    case NoiseVerdict::Deduplicate:   return "Deduplicate";
    case NoiseVerdict::Coalesce:      return "Coalesce";
    case NoiseVerdict::RateLimited:   return "RateLimited";
    case NoiseVerdict::BelowThreshold: return "BelowThreshold";
    case NoiseVerdict::Aggregated:    return "Aggregated";
    case NoiseVerdict::Backpressured: return "Backpressured";
    case NoiseVerdict::Quarantined:   return "Quarantined";
  }
  return "Unknown";
}

bool CollectorNoisePolicy::isValid() const {
  return !collector_name.empty();
}

bool NoiseEvent::isValid() const {
  return !event_id.empty() && !collector.empty();
}

bool NoiseResult::passes() const {
  return verdict == NoiseVerdict::Pass;
}

std::string NoiseResult::summary() const {
  std::string result = std::string(NoiseVerdictName(verdict));
  if (!reason.empty()) result += " (" + reason + ")";
  if (batch_size > 1) result += " batch=" + std::to_string(batch_size);
  return result;
}

NoiseControlEngine::NoiseControlEngine() {}
NoiseControlEngine::~NoiseControlEngine() {}

void NoiseControlEngine::setPolicy(const CollectorNoisePolicy& policy) {
  std::lock_guard<std::mutex> lock(mu_);
  policies_[policy.collector_name] = policy;
}

CollectorNoisePolicy NoiseControlEngine::getPolicy(const std::string& collector) const {
  std::lock_guard<std::mutex> lock(mu_);
  auto it = policies_.find(collector);
  if (it == policies_.end()) return CollectorNoisePolicy{};
  return it->second;
}

NoiseResult NoiseControlEngine::evaluate(const NoiseEvent& event) {
  total_events_++;
  if (!event.isValid()) {
    total_filtered_++;
    return {NoiseVerdict::Pass, "invalid event", 0};
  }

  std::lock_guard<std::mutex> lock(mu_);
  auto it = policies_.find(event.collector);
  if (it == policies_.end()) {
    return {NoiseVerdict::Pass, "no policy", 0};
  }

  const auto& policy = it->second;
  auto& state = states_[event.collector];

  if (state.quarantined) {
    if (hasMechanism(policy.mechanisms, NoiseMechanism::Quarantine)) {
      total_filtered_++;
      return {NoiseVerdict::Quarantined, "collector quarantined", 0};
    }
  }

  if (hasMechanism(policy.mechanisms, NoiseMechanism::Deduplication)) {
    auto result = evaluateDedup(event, policy);
    if (!result.passes()) return result;
  }

  if (hasMechanism(policy.mechanisms, NoiseMechanism::RateLimiting)) {
    auto result = evaluateRateLimit(event, policy);
    if (!result.passes()) return result;
  }

  if (hasMechanism(policy.mechanisms, NoiseMechanism::Threshold)) {
    auto result = evaluateThreshold(event, policy);
    if (!result.passes()) return result;
  }

  return {NoiseVerdict::Pass, "", 0};
}

NoiseResult NoiseControlEngine::evaluateDedup(const NoiseEvent& event, const CollectorNoisePolicy& policy) {
  auto& state = states_[event.collector];
  auto now_ms = event.timestamp_ms;

  for (const auto& [eid, ts] : state.recent_events) {
    if (eid == event.event_id && (now_ms - ts) < static_cast<std::int64_t>(policy.dedup_window_ms)) {
      total_filtered_++;
      return {NoiseVerdict::Deduplicate, "duplicate within window", 0};
    }
  }

  state.recent_events.push_back({event.event_id, now_ms});
  auto cutoff = now_ms - static_cast<std::int64_t>(policy.dedup_window_ms);
  state.recent_events.erase(
    std::remove_if(state.recent_events.begin(), state.recent_events.end(),
      [cutoff](const auto& p) { return p.second < cutoff; }),
    state.recent_events.end());

  return {NoiseVerdict::Pass, "", 0};
}

NoiseResult NoiseControlEngine::evaluateRateLimit(const NoiseEvent& event, const CollectorNoisePolicy& policy) {
  auto& state = states_[event.collector];
  auto now_ms = event.timestamp_ms;

  if (now_ms - state.rate_window_start > static_cast<std::int64_t>(policy.rate_limit_window_ms)) {
    state.rate_count = 0;
    state.rate_window_start = now_ms;
  }

  state.rate_count++;
  if (state.rate_count > policy.rate_limit_max) {
    total_filtered_++;
    return {NoiseVerdict::RateLimited, "exceeded rate limit", 0};
  }

  return {NoiseVerdict::Pass, "", 0};
}

NoiseResult NoiseControlEngine::evaluateThreshold(const NoiseEvent& event, const CollectorNoisePolicy& policy) {
  if (event.numeric_value < policy.threshold_min) {
    total_filtered_++;
    return {NoiseVerdict::BelowThreshold, "below threshold", 0};
  }
  return {NoiseVerdict::Pass, "", 0};
}

std::size_t NoiseControlEngine::totalEvents() const {
  return total_events_.load();
}

std::size_t NoiseControlEngine::totalFiltered() const {
  return total_filtered_.load();
}

std::size_t NoiseControlEngine::totalPassed() const {
  return total_events_.load() - total_filtered_.load();
}

void NoiseControlEngine::reset(const std::string& collector) {
  std::lock_guard<std::mutex> lock(mu_);
  states_.erase(collector);
}

void NoiseControlEngine::resetAll() {
  std::lock_guard<std::mutex> lock(mu_);
  states_.clear();
}

}  // namespace monix::collectors::noise
