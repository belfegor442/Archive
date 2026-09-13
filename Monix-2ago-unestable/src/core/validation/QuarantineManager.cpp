#include "QuarantineManager.hpp"

#include <algorithm>

namespace monix::validation {

QuarantineManager::QuarantineManager(QuarantineConfig config)
  : config_(config) {}

void QuarantineManager::quarantine(const events::Event& event,
                                    const ValidationResult& result,
                                    events::Timestamp now) {
  std::lock_guard<std::mutex> lock(mu_);

  totalQuarantined_++;

  if (entries_.size() >= config_.max_events) {
    entries_.erase(entries_.begin());
  }

  QuarantineEntry entry;
  entry.event_id = event.id.toString();
  entry.status = result.status;
  entry.issues = result.issues;
  entry.source = event.source.id;
  entry.event_type = event.type.qualifiedName();
  entry.quarantined_at = now;
  entries_.push_back(std::move(entry));

  for (const auto& issue : result.issues) {
    if (issue.severity != ValidationSeverity::Error &&
        issue.severity != ValidationSeverity::Fatal) continue;

    std::string key = event.source.id + ":" +
                      std::to_string(static_cast<std::uint16_t>(issue.code)) + ":" +
                      issue.field;

    auto it = aggregates_.find(key);
    if (it != aggregates_.end()) {
      it->second.count++;
      it->second.last_seen = now;
    } else {
      if (aggregates_.size() >= config_.max_aggregates) {
        aggregates_.erase(aggregates_.begin());
      }
      QuarantineAggregate agg;
      agg.source = event.source.id;
      agg.code = issue.code;
      agg.field = issue.field;
      agg.count = 1;
      agg.representative_event_id = event.id.toString();
      agg.first_seen = now;
      agg.last_seen = now;
      aggregates_[std::move(key)] = std::move(agg);
    }
  }
}

std::vector<QuarantineEntry> QuarantineManager::drain() {
  std::lock_guard<std::mutex> lock(mu_);
  auto result = std::move(entries_);
  entries_.clear();
  return result;
}

std::vector<QuarantineAggregate> QuarantineManager::aggregates() const {
  std::lock_guard<std::mutex> lock(mu_);
  std::vector<QuarantineAggregate> result;
  result.reserve(aggregates_.size());
  for (const auto& [key, agg] : aggregates_) {
    result.push_back(agg);
  }
  return result;
}

std::size_t QuarantineManager::size() const {
  std::lock_guard<std::mutex> lock(mu_);
  return entries_.size();
}

std::size_t QuarantineManager::aggregateCount() const {
  std::lock_guard<std::mutex> lock(mu_);
  return aggregates_.size();
}

void QuarantineManager::clear() {
  std::lock_guard<std::mutex> lock(mu_);
  entries_.clear();
  aggregates_.clear();
}

void QuarantineManager::purgeExpired(events::Timestamp now) {
  std::lock_guard<std::mutex> lock(mu_);
  auto cutoff = now - config_.retention.count() * 1000;
  entries_.erase(
    std::remove_if(entries_.begin(), entries_.end(),
      [cutoff](const QuarantineEntry& e) { return e.quarantined_at < cutoff; }),
    entries_.end());
}

std::uint64_t QuarantineManager::totalQuarantined() const {
  std::lock_guard<std::mutex> lock(mu_);
  return totalQuarantined_;
}

}  // namespace monix::validation
