#pragma once

#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace monix::collectors::quarantine {

enum class ValidationVerdict : std::uint8_t {
  Valid,
  Warning,
  Invalid,
  Fatal
};

const char* ValidationVerdictName(ValidationVerdict v);

struct CollectorEvent {
  std::string event_id;
  std::string collector;
  std::string event_type;
  std::string source;
  std::int64_t timestamp_ms = 0;

  bool isValid() const;
};

struct ValidationResult {
  ValidationVerdict verdict = ValidationVerdict::Valid;
  std::string reason;
  std::vector<std::string> issues;

  bool isValid() const;
  bool isQuarantined() const;
  std::string summary() const;
};

struct QuarantineEntry {
  std::string event_id;
  std::string status;
  std::vector<std::string> issues;
  std::string source;
  std::string event_type;
  std::int64_t quarantined_at_ms = 0;

  bool isValid() const;
};

struct QuarantineAggregate {
  std::string source;
  std::string field;
  std::uint64_t count = 0;
  std::string representative_event_id;
  std::int64_t first_seen_ms = 0;
  std::int64_t last_seen_ms = 0;

  bool isValid() const;
};

struct QuarantineConfig {
  std::size_t max_events = 100000;
  std::size_t max_aggregates = 10000;
  std::int64_t retention_ms = 3600000;

  bool isValid() const;
};

struct QuarantineSummary {
  std::size_t total_received = 0;
  std::size_t total_valid = 0;
  std::size_t total_warnings = 0;
  std::size_t total_invalid = 0;
  std::size_t total_fatal = 0;
  std::size_t total_quarantined = 0;

  double invalidRate() const;
  std::string summary() const;
};

using ValidationCallback = std::function<void(const CollectorEvent&, const ValidationResult&)>;
using QuarantineCallback = std::function<void(const QuarantineEntry&)>;

class QuarantineIntegration {
public:
  QuarantineIntegration();
  explicit QuarantineIntegration(const QuarantineConfig& cfg);
  ~QuarantineIntegration();

  QuarantineIntegration(const QuarantineIntegration&) = delete;
  QuarantineIntegration& operator=(const QuarantineIntegration&) = delete;

  void setValidationCallback(ValidationCallback cb);
  void setQuarantineCallback(QuarantineCallback cb);

  void setConfig(const QuarantineConfig& cfg);

  ValidationResult validate(const CollectorEvent& event);
  bool quarantine(const CollectorEvent& event, const ValidationResult& result);

  std::vector<QuarantineEntry> drain(std::size_t max_count = 0);
  std::vector<QuarantineAggregate> aggregates() const;

  QuarantineSummary summary() const;
  std::size_t quarantinedCount() const;
  std::size_t aggregateCount() const;
  std::size_t totalQuarantined() const;

  void purgeExpired(std::int64_t now_ms = 0);
  void clear();

private:
  ValidationCallback validation_callback_;
  QuarantineCallback quarantine_callback_;
  mutable std::mutex mu_;
  QuarantineConfig config_;
  std::vector<QuarantineEntry> entries_;
  std::unordered_map<std::string, QuarantineAggregate> aggregates_;
  QuarantineSummary summary_;
  std::size_t total_quarantined_{0};
};

}  // namespace monix::collectors::quarantine
