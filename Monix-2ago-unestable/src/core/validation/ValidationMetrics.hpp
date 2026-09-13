#pragma once

#include "ValidationTypes.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <mutex>

namespace monix::validation {

struct ValidationMetrics {
  std::uint64_t events_validated = 0;
  std::uint64_t events_valid = 0;
  std::uint64_t events_warnings = 0;
  std::uint64_t events_invalid = 0;
  std::uint64_t validation_failures = 0;
  std::uint64_t quarantined = 0;
  std::uint64_t schema_failures = 0;
  std::uint64_t semantic_failures = 0;
  std::uint64_t integrity_failures = 0;
  double validation_latency_us = 0.0;
};

struct SourceValidationMetrics {
  std::uint64_t received = 0;
  std::uint64_t valid = 0;
  std::uint64_t warnings = 0;
  std::uint64_t invalid = 0;

  double invalidRate() const;
};

struct CollectorValidationHealth {
  std::uint64_t events_received = 0;
  std::uint64_t valid = 0;
  std::uint64_t warnings = 0;
  std::uint64_t invalid = 0;
  double invalid_rate = 0.0;
};

class ValidationMetricsTracker {
public:
  void recordValidated(ValidationStatus status, double latencyUs);
  void recordQuarantined();
  void recordSchemaFailure();
  void recordSemanticFailure();
  void recordIntegrityFailure();
  void recordSource(const std::string& source, ValidationStatus status);

  ValidationMetrics snapshot() const;
  SourceValidationMetrics sourceMetrics(const std::string& source) const;
  std::unordered_map<std::string, SourceValidationMetrics> allSourceMetrics() const;
  CollectorValidationHealth health(const std::string& source) const;
  void reset();

private:
  mutable std::mutex mu_;
  ValidationMetrics metrics_;
  std::unordered_map<std::string, SourceValidationMetrics> sourceMap_;
};

}  // namespace monix::validation
