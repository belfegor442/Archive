#include "RetentionPolicy.hpp"

#include <algorithm>
#include <chrono>

namespace monix::collectors::retention {

const char* RetentionModeName(RetentionMode m) {
  switch (m) {
    case RetentionMode::Delete:  return "Delete";
    case RetentionMode::Archive: return "Archive";
    case RetentionMode::Compact: return "Compact";
  }
  return "Unknown";
}

bool RetentionRule::isValid() const {
  return !name.empty();
}

bool RetentionRule::matches(const eventstore::StoredEvent& event, std::int64_t now_ms) const {
  if (!enabled) return false;
  if (!event_type.empty() && event.event_type != event_type) return false;
  if (static_cast<std::uint8_t>(event.severity) < static_cast<std::uint8_t>(min_severity)) return false;
  if (static_cast<std::uint8_t>(event.severity) > static_cast<std::uint8_t>(max_severity)) return false;
  if (max_age_ms > 0 && (now_ms - event.timestamp_ms) > static_cast<std::int64_t>(max_age_ms)) return true;
  return false;
}

std::string RetentionRule::summary() const {
  return name + " mode=" + std::string(RetentionModeName(mode)) +
    " age=" + std::to_string(max_age_ms) + "ms";
}

bool RetentionConfig::isValid() const {
  if (global_max_age_ms == 0) return false;
  if (global_max_events == 0) return false;
  return true;
}

std::string RetentionResult::summary() const {
  return "removed=" + std::to_string(events_removed) +
    " kept=" + std::to_string(events_kept) +
    " rules=" + std::to_string(rules_applied);
}

RetentionPolicy::RetentionPolicy() {}
RetentionPolicy::RetentionPolicy(const RetentionConfig& cfg) : config_(cfg) {}
RetentionPolicy::~RetentionPolicy() {}

void RetentionPolicy::setConfig(const RetentionConfig& cfg) {
  std::lock_guard<std::mutex> lock(mu_);
  config_ = cfg;
}

RetentionConfig RetentionPolicy::getConfig() const {
  std::lock_guard<std::mutex> lock(mu_);
  return config_;
}

void RetentionPolicy::addRule(const RetentionRule& rule) {
  std::lock_guard<std::mutex> lock(mu_);
  config_.rules.push_back(rule);
}

bool RetentionPolicy::removeRule(const std::string& name) {
  std::lock_guard<std::mutex> lock(mu_);
  auto it = std::remove_if(config_.rules.begin(), config_.rules.end(),
    [&](const RetentionRule& r) { return r.name == name; });
  if (it != config_.rules.end()) {
    config_.rules.erase(it, config_.rules.end());
    return true;
  }
  return false;
}

RetentionResult RetentionPolicy::enforce(eventstore::EventStorage& storage) {
  std::lock_guard<std::mutex> lock(mu_);
  RetentionResult result;
  result.executed_at_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::system_clock::now().time_since_epoch()).count();

  if (!config_.enabled) return result;

  auto now_ms = result.executed_at_ms;

  for (const auto& rule : config_.rules) {
    if (!rule.enabled) continue;
    result.rules_applied++;

    eventstore::QueryFilter filter;
    filter.event_type = rule.event_type;
    filter.min_severity = rule.min_severity;
    filter.max_severity = rule.max_severity;

    auto events = storage.query(filter);
    std::size_t removed = 0;
    for (const auto& e : events) {
      if (rule.matches(e, now_ms)) {
        removed++;
      }
    }
    result.events_removed += removed;
    result.events_kept += (events.size() - removed);
  }

  return result;
}

std::size_t RetentionPolicy::ruleCount() const {
  std::lock_guard<std::mutex> lock(mu_);
  return config_.rules.size();
}

std::vector<RetentionRule> RetentionPolicy::activeRules() const {
  std::lock_guard<std::mutex> lock(mu_);
  std::vector<RetentionRule> result;
  for (const auto& rule : config_.rules) {
    if (rule.enabled) result.push_back(rule);
  }
  return result;
}

}  // namespace monix::collectors::retention
