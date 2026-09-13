#pragma once

#include "ValidationTypes.hpp"

#include <vector>
#include <chrono>
#include <string>

namespace monix::validation {

struct ValidationResult {
  ValidationStatus status = ValidationStatus::Valid;
  std::vector<ValidationIssue> issues;
  std::string validator_version = "3.0.0";
  std::chrono::microseconds duration{0};

  void addIssue(ValidationIssueCode code, ValidationSeverity sev,
                const std::string& field, const std::string& message);
  void mergeFrom(const ValidationResult& other);
  bool isValid() const;
  bool hasWarnings() const;
  bool isInvalid() const;
};

struct ValidationReport {
  std::string event_id;
  ValidationStatus status = ValidationStatus::Valid;
  std::vector<ValidationIssue> issues;
  std::string validator_version = "3.0.0";
  std::string schema_version;
  std::chrono::microseconds duration{0};
};

}  // namespace monix::validation
