#include "ValidationMetrics.hpp"

namespace monix::validation {

double SourceValidationMetrics::invalidRate() const {
  if (received == 0) return 0.0;
  return static_cast<double>(invalid) / static_cast<double>(received);
}

void ValidationMetricsTracker::recordValidated(ValidationStatus status, double latencyUs) {
  std::lock_guard<std::mutex> lock(mu_);
  metrics_.events_validated++;
  metrics_.validation_latency_us =
    (metrics_.validation_latency_us * (metrics_.events_validated - 1) + latencyUs) /
    metrics_.events_validated;
  switch (status) {
    case ValidationStatus::Valid:
      metrics_.events_valid++;
      break;
    case ValidationStatus::ValidWithWarnings:
      metrics_.events_warnings++;
      break;
    case ValidationStatus::Invalid:
      metrics_.events_invalid++;
      break;
  }
}

void ValidationMetricsTracker::recordQuarantined() {
  std::lock_guard<std::mutex> lock(mu_);
  metrics_.quarantined++;
}

void ValidationMetricsTracker::recordSchemaFailure() {
  std::lock_guard<std::mutex> lock(mu_);
  metrics_.schema_failures++;
}

void ValidationMetricsTracker::recordSemanticFailure() {
  std::lock_guard<std::mutex> lock(mu_);
  metrics_.semantic_failures++;
}

void ValidationMetricsTracker::recordIntegrityFailure() {
  std::lock_guard<std::mutex> lock(mu_);
  metrics_.integrity_failures++;
}

void ValidationMetricsTracker::recordSource(const std::string& source, ValidationStatus status) {
  std::lock_guard<std::mutex> lock(mu_);
  auto& sm = sourceMap_[source];
  sm.received++;
  switch (status) {
    case ValidationStatus::Valid:
      sm.valid++;
      break;
    case ValidationStatus::ValidWithWarnings:
      sm.warnings++;
      break;
    case ValidationStatus::Invalid:
      sm.invalid++;
      break;
  }
}

ValidationMetrics ValidationMetricsTracker::snapshot() const {
  std::lock_guard<std::mutex> lock(mu_);
  return metrics_;
}

SourceValidationMetrics ValidationMetricsTracker::sourceMetrics(const std::string& source) const {
  std::lock_guard<std::mutex> lock(mu_);
  auto it = sourceMap_.find(source);
  if (it != sourceMap_.end()) return it->second;
  return SourceValidationMetrics{};
}

std::unordered_map<std::string, SourceValidationMetrics>
ValidationMetricsTracker::allSourceMetrics() const {
  std::lock_guard<std::mutex> lock(mu_);
  return sourceMap_;
}

CollectorValidationHealth ValidationMetricsTracker::health(const std::string& source) const {
  std::lock_guard<std::mutex> lock(mu_);
  CollectorValidationHealth h;
  auto it = sourceMap_.find(source);
  if (it != sourceMap_.end()) {
    h.events_received = it->second.received;
    h.valid = it->second.valid;
    h.warnings = it->second.warnings;
    h.invalid = it->second.invalid;
    h.invalid_rate = it->second.invalidRate();
  }
  return h;
}

void ValidationMetricsTracker::reset() {
  std::lock_guard<std::mutex> lock(mu_);
  metrics_ = ValidationMetrics{};
  sourceMap_.clear();
}

}  // namespace monix::validation
