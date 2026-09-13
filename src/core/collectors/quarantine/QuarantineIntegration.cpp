#include "QuarantineIntegration.hpp"

#include <algorithm>
#include <chrono>

namespace monix::collectors::quarantine {

const char* ValidationVerdictName(ValidationVerdict v) {
  switch (v) {
    case ValidationVerdict::Valid:   return "Valid";
    case ValidationVerdict::Warning: return "Warning";
    case ValidationVerdict::Invalid: return "Invalid";
    case ValidationVerdict::Fatal:   return "Fatal";
  }
  return "Unknown";
}

bool CollectorEvent::isValid() const {
  return !event_id.empty() && !collector.empty();
}

bool ValidationResult::isValid() const {
  return verdict == ValidationVerdict::Valid;
}

bool ValidationResult::isQuarantined() const {
  return verdict == ValidationVerdict::Invalid || verdict == ValidationVerdict::Fatal;
}

std::string ValidationResult::summary() const {
  std::string result = std::string(ValidationVerdictName(verdict));
  if (!reason.empty()) result += " (" + reason + ")";
  result += " issues=" + std::to_string(issues.size());
  return result;
}

bool QuarantineEntry::isValid() const {
  return !event_id.empty();
}

bool QuarantineAggregate::isValid() const {
  return !source.empty();
}

bool QuarantineConfig::isValid() const {
  if (max_events == 0) return false;
  if (max_aggregates == 0) return false;
  if (retention_ms <= 0) return false;
  return true;
}

double QuarantineSummary::invalidRate() const {
  if (total_received == 0) return 0.0;
  return static_cast<double>(total_invalid + total_fatal) / static_cast<double>(total_received);
}

std::string QuarantineSummary::summary() const {
  return "received=" + std::to_string(total_received) +
    " valid=" + std::to_string(total_valid) +
    " warn=" + std::to_string(total_warnings) +
    " invalid=" + std::to_string(total_invalid) +
    " fatal=" + std::to_string(total_fatal) +
    " quarantined=" + std::to_string(total_quarantined);
}

QuarantineIntegration::QuarantineIntegration() {}
QuarantineIntegration::QuarantineIntegration(const QuarantineConfig& cfg) : config_(cfg) {}
QuarantineIntegration::~QuarantineIntegration() {}

void QuarantineIntegration::setValidationCallback(ValidationCallback cb) {
  std::lock_guard<std::mutex> lock(mu_);
  validation_callback_ = std::move(cb);
}

void QuarantineIntegration::setQuarantineCallback(QuarantineCallback cb) {
  std::lock_guard<std::mutex> lock(mu_);
  quarantine_callback_ = std::move(cb);
}

void QuarantineIntegration::setConfig(const QuarantineConfig& cfg) {
  std::lock_guard<std::mutex> lock(mu_);
  config_ = cfg;
}

ValidationResult QuarantineIntegration::validate(const CollectorEvent& event) {
  ValidationResult result;
  result.verdict = ValidationVerdict::Valid;

  if (!event.isValid()) {
    result.verdict = ValidationVerdict::Invalid;
    result.reason = "invalid event structure";
    result.issues.push_back("missing required fields");
  }

  if (event.event_type.empty()) {
    if (result.verdict == ValidationVerdict::Valid) {
      result.verdict = ValidationVerdict::Warning;
    }
    result.reason = "missing event type";
    result.issues.push_back("event_type is empty");
  }

  {
    std::lock_guard<std::mutex> lock(mu_);
    summary_.total_received++;
    if (result.verdict == ValidationVerdict::Valid) summary_.total_valid++;
    else if (result.verdict == ValidationVerdict::Warning) summary_.total_warnings++;
    else if (result.verdict == ValidationVerdict::Invalid) summary_.total_invalid++;
    else if (result.verdict == ValidationVerdict::Fatal) summary_.total_fatal++;
  }

  if (validation_callback_) {
    validation_callback_(event, result);
  }

  return result;
}

bool QuarantineIntegration::quarantine(const CollectorEvent& event, const ValidationResult& result) {
  if (!result.isQuarantined()) return false;

  QuarantineEntry entry;
  entry.event_id = event.event_id;
  entry.status = (result.verdict == ValidationVerdict::Fatal) ? "fatal" : "invalid";
  entry.issues = result.issues;
  entry.source = event.collector;
  entry.event_type = event.event_type;
  entry.quarantined_at_ms = event.timestamp_ms;

  {
    std::lock_guard<std::mutex> lock(mu_);

    if (entries_.size() >= config_.max_events) {
      entries_.erase(entries_.begin());
    }
    entries_.push_back(entry);

    for (const auto& issue : result.issues) {
      std::string key = event.collector + ":" + issue;
      auto it = aggregates_.find(key);
      if (it != aggregates_.end()) {
        it->second.count++;
        it->second.last_seen_ms = event.timestamp_ms;
      } else {
        if (aggregates_.size() >= config_.max_aggregates) {
          aggregates_.erase(aggregates_.begin());
        }
        QuarantineAggregate agg;
        agg.source = event.collector;
        agg.field = issue;
        agg.count = 1;
        agg.representative_event_id = event.event_id;
        agg.first_seen_ms = event.timestamp_ms;
        agg.last_seen_ms = event.timestamp_ms;
        aggregates_[key] = agg;
      }
    }

    summary_.total_quarantined++;
    total_quarantined_++;
  }

  if (quarantine_callback_) {
    quarantine_callback_(entry);
  }

  return true;
}

std::vector<QuarantineEntry> QuarantineIntegration::drain(std::size_t max_count) {
  std::lock_guard<std::mutex> lock(mu_);
  if (max_count == 0 || max_count >= entries_.size()) {
    auto result = std::move(entries_);
    entries_.clear();
    return result;
  }
  std::vector<QuarantineEntry> result(entries_.begin(), entries_.begin() + max_count);
  entries_.erase(entries_.begin(), entries_.begin() + max_count);
  return result;
}

std::vector<QuarantineAggregate> QuarantineIntegration::aggregates() const {
  std::lock_guard<std::mutex> lock(mu_);
  std::vector<QuarantineAggregate> result;
  result.reserve(aggregates_.size());
  for (const auto& [key, agg] : aggregates_) {
    result.push_back(agg);
  }
  return result;
}

QuarantineSummary QuarantineIntegration::summary() const {
  std::lock_guard<std::mutex> lock(mu_);
  return summary_;
}

std::size_t QuarantineIntegration::quarantinedCount() const {
  std::lock_guard<std::mutex> lock(mu_);
  return entries_.size();
}

std::size_t QuarantineIntegration::aggregateCount() const {
  std::lock_guard<std::mutex> lock(mu_);
  return aggregates_.size();
}

std::size_t QuarantineIntegration::totalQuarantined() const {
  std::lock_guard<std::mutex> lock(mu_);
  return total_quarantined_;
}

void QuarantineIntegration::purgeExpired(std::int64_t now_ms) {
  if (now_ms == 0) {
    now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count();
  }
  std::lock_guard<std::mutex> lock(mu_);
  auto cutoff = now_ms - config_.retention_ms;
  entries_.erase(
    std::remove_if(entries_.begin(), entries_.end(),
      [cutoff](const QuarantineEntry& e) { return e.quarantined_at_ms < cutoff; }),
    entries_.end());
}

void QuarantineIntegration::clear() {
  std::lock_guard<std::mutex> lock(mu_);
  entries_.clear();
  aggregates_.clear();
}

}  // namespace monix::collectors::quarantine
