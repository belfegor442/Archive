#include "RestartPolicy.hpp"

#include <algorithm>
#include <cmath>

namespace monix::collectors::health {

bool RestartPolicyConfig::isValid() const {
  if (auto_restart && max_restart_attempts == 0) return false;
  if (initial_backoff_ms <= 0) return false;
  if (backoff_multiplier < 1.0) return false;
  if (max_backoff_ms < initial_backoff_ms) return false;
  return true;
}

const char* RestartActionName(RestartAction a) {
  switch (a) {
    case RestartAction::None:   return "None";
    case RestartAction::Restart: return "Restart";
    case RestartAction::Reject: return "Reject";
  }
  return "Unknown";
}

bool RestartDecision::shouldRestart() const {
  return action == RestartAction::Restart;
}

std::string RestartDecision::summary() const {
  std::string result = std::string(RestartActionName(action));
  result += " attempt=" + std::to_string(attempt);
  result += " backoff=" + std::to_string(backoff_ms) + "ms";
  if (!reason.empty()) result += " (" + reason + ")";
  return result;
}

RestartPolicy::RestartPolicy() {}
RestartPolicy::RestartPolicy(const RestartPolicyConfig& cfg) : config_(cfg) {}
RestartPolicy::~RestartPolicy() {}

void RestartPolicy::setConfig(const RestartPolicyConfig& cfg) {
  std::lock_guard<std::mutex> lock(mu_);
  config_ = cfg;
}

RestartPolicyConfig RestartPolicy::getConfig() const {
  std::lock_guard<std::mutex> lock(mu_);
  return config_;
}

std::int64_t RestartPolicy::calculateBackoff(std::size_t attempt) const {
  double backoff = static_cast<double>(config_.initial_backoff_ms);
  for (std::size_t i = 1; i < attempt; i++) {
    backoff *= config_.backoff_multiplier;
  }
  backoff = std::min(backoff, static_cast<double>(config_.max_backoff_ms));
  return static_cast<std::int64_t>(backoff);
}

RestartDecision RestartPolicy::recordFailure(const std::string& collector) {
  std::lock_guard<std::mutex> lock(mu_);
  auto& cs = states_[collector];
  cs.consecutive_failures++;
  cs.total_restarts++;

  RestartDecision decision;
  decision.attempt = cs.consecutive_failures;

  if (!config_.auto_restart) {
    decision.action = RestartAction::None;
    decision.reason = "auto_restart disabled";
    return decision;
  }

  if (cs.exhausted) {
    decision.action = RestartAction::Reject;
    decision.reason = "exhausted";
    return decision;
  }

  if (cs.consecutive_failures > config_.max_restart_attempts) {
    cs.exhausted = true;
    decision.action = RestartAction::Reject;
    decision.reason = "max attempts exceeded";
    return decision;
  }

  decision.backoff_ms = calculateBackoff(cs.consecutive_failures);
  cs.last_backoff_ms = decision.backoff_ms;
  decision.action = RestartAction::Restart;
  return decision;
}

void RestartPolicy::recordSuccess(const std::string& collector) {
  std::lock_guard<std::mutex> lock(mu_);
  auto it = states_.find(collector);
  if (it != states_.end()) {
    it->second.consecutive_failures = 0;
    it->second.exhausted = false;
  }
}

std::size_t RestartPolicy::restartCount(const std::string& collector) const {
  std::lock_guard<std::mutex> lock(mu_);
  auto it = states_.find(collector);
  if (it == states_.end()) return 0;
  return it->second.total_restarts;
}

std::int64_t RestartPolicy::lastBackoffMs(const std::string& collector) const {
  std::lock_guard<std::mutex> lock(mu_);
  auto it = states_.find(collector);
  if (it == states_.end()) return 0;
  return it->second.last_backoff_ms;
}

bool RestartPolicy::isExhausted(const std::string& collector) const {
  std::lock_guard<std::mutex> lock(mu_);
  auto it = states_.find(collector);
  if (it == states_.end()) return false;
  return it->second.exhausted;
}

void RestartPolicy::reset(const std::string& collector) {
  std::lock_guard<std::mutex> lock(mu_);
  states_.erase(collector);
}

void RestartPolicy::resetAll() {
  std::lock_guard<std::mutex> lock(mu_);
  states_.clear();
}

}  // namespace monix::collectors::health
