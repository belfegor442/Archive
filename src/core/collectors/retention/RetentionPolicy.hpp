#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "EventStorage.hpp"

namespace monix::collectors::retention {

enum class RetentionMode : std::uint8_t {
  Delete,
  Archive,
  Compact
};

const char* RetentionModeName(RetentionMode m);

struct RetentionRule {
  std::string name;
  RetentionMode mode = RetentionMode::Delete;
  std::int64_t max_age_ms = 0;
  std::size_t max_count = 0;
  std::size_t max_size_bytes = 0;
  eventstore::EventSeverity min_severity = eventstore::EventSeverity::Debug;
  eventstore::EventSeverity max_severity = eventstore::EventSeverity::Critical;
  std::string event_type;
  bool enabled = true;

  bool isValid() const;
  bool matches(const eventstore::StoredEvent& event, std::int64_t now_ms) const;
  std::string summary() const;
};

struct RetentionConfig {
  std::vector<RetentionRule> rules;
  std::int64_t global_max_age_ms = 7 * 24 * 3600000;
  std::size_t global_max_events = 1000000;
  std::int64_t check_interval_ms = 3600000;
  bool enabled = true;

  bool isValid() const;
};

struct RetentionResult {
  std::size_t events_removed = 0;
  std::size_t events_kept = 0;
  std::size_t rules_applied = 0;
  std::int64_t executed_at_ms = 0;

  std::string summary() const;
};

class RetentionPolicy {
public:
  RetentionPolicy();
  explicit RetentionPolicy(const RetentionConfig& cfg);
  ~RetentionPolicy();

  RetentionPolicy(const RetentionPolicy&) = delete;
  RetentionPolicy& operator=(const RetentionPolicy&) = delete;

  void setConfig(const RetentionConfig& cfg);
  RetentionConfig getConfig() const;

  void addRule(const RetentionRule& rule);
  bool removeRule(const std::string& name);

  RetentionResult enforce(eventstore::EventStorage& storage);

  std::size_t ruleCount() const;
  std::vector<RetentionRule> activeRules() const;

private:
  RetentionConfig config_;
  mutable std::mutex mu_;
};

}  // namespace monix::collectors::retention
