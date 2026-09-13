#include "ValidationResult.hpp"

namespace monix::validation {

void ValidationResult::addIssue(ValidationIssueCode code, ValidationSeverity sev,
                                const std::string& field, const std::string& message) {
  ValidationIssue issue;
  issue.code = code;
  issue.severity = sev;
  issue.field = field;
  issue.message = message;
  issues.push_back(std::move(issue));
}

void ValidationResult::mergeFrom(const ValidationResult& other) {
  for (const auto& issue : other.issues) {
    issues.push_back(issue);
  }
  if (other.status == ValidationStatus::Invalid) {
    status = ValidationStatus::Invalid;
  } else if (other.status == ValidationStatus::ValidWithWarnings &&
             status == ValidationStatus::Valid) {
    status = ValidationStatus::ValidWithWarnings;
  }
}

bool ValidationResult::isValid() const {
  return status != ValidationStatus::Invalid;
}

bool ValidationResult::hasWarnings() const {
  return status == ValidationStatus::ValidWithWarnings;
}

bool ValidationResult::isInvalid() const {
  return status == ValidationStatus::Invalid;
}

}  // namespace monix::validation
