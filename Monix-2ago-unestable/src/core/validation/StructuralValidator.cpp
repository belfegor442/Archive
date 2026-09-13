#include "StructuralValidator.hpp"

#include <chrono>

namespace monix::validation {

ValidationResult StructuralValidator::validate(const events::Event& event,
                                               const ValidationContext& ctx) {
  auto start = std::chrono::steady_clock::now();
  ValidationResult result;

  if (!event.id.isValid()) {
    result.addIssue(ValidationIssueCode::MissingEventId,
                    ValidationSeverity::Fatal, "id", "Event ID is missing or invalid");
  }

  if (event.type.namespace_name.empty() || event.type.name.empty()) {
    result.addIssue(ValidationIssueCode::MissingEventType,
                    ValidationSeverity::Fatal, "type", "Event type is missing");
  }

  if (!event.time.isValid()) {
    result.addIssue(ValidationIssueCode::MissingTimestamp,
                    ValidationSeverity::Fatal, "time", "Timestamp is missing (occurrence or ingestion)");
  }

  if (!event.source.isValid()) {
    result.addIssue(ValidationIssueCode::MissingSource,
                    ValidationSeverity::Fatal, "source", "Source is missing or empty");
  }

  if (!event.provenance.isValid()) {
    result.addIssue(ValidationIssueCode::MissingProvenance,
                    ValidationSeverity::Error, "provenance", "Provenance source_id is missing");
  }

  auto end = std::chrono::steady_clock::now();
  result.duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

  bool hasFatal = false;
  for (const auto& issue : result.issues) {
    if (issue.severity == ValidationSeverity::Fatal) {
      hasFatal = true;
      break;
    }
  }
  result.status = hasFatal ? ValidationStatus::Invalid
                           : (result.issues.empty() ? ValidationStatus::Valid
                                                     : ValidationStatus::ValidWithWarnings);

  return result;
}

}  // namespace monix::validation
