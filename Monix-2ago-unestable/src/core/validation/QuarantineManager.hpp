#pragma once

#include "ValidationTypes.hpp"
#include "ValidationResult.hpp"
#include "../events/Event.hpp"
#include "../events/EventId.hpp"

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <chrono>

namespace monix::validation {

struct QuarantineEntry {
  std::string event_id;
  ValidationStatus status = ValidationStatus::Invalid;
  std::vector<ValidationIssue> issues;
  std::string source;
  std::string event_type;
  events::Timestamp quarantined_at = 0;
};

struct QuarantineAggregate {
  std::string source;
  ValidationIssueCode code = ValidationIssueCode::None;
  std::string field;
  std::uint64_t count = 0;
  std::string representative_event_id;
  events::Timestamp first_seen = 0;
  events::Timestamp last_seen = 0;
};

struct QuarantineConfig {
  std::size_t max_events = 100000;
  std::size_t max_aggregates = 10000;
  std::chrono::seconds retention{3600};
};

class QuarantineManager {
public:
  explicit QuarantineManager(QuarantineConfig config = QuarantineConfig{});

  void quarantine(const events::Event& event,
                  const ValidationResult& result,
                  events::Timestamp now);

  std::vector<QuarantineEntry> drain();
  std::vector<QuarantineAggregate> aggregates() const;
  std::size_t size() const;
  std::size_t aggregateCount() const;
  void clear();
  void purgeExpired(events::Timestamp now);

  std::uint64_t totalQuarantined() const;

private:
  QuarantineConfig config_;
  mutable std::mutex mu_;
  std::vector<QuarantineEntry> entries_;
  std::unordered_map<std::string, QuarantineAggregate> aggregates_;
  std::uint64_t totalQuarantined_ = 0;
};

}  // namespace monix::validation
