#include "SchemaValidator.hpp"

namespace monix::validation {

ValidationResult SchemaValidator::validate(const events::Event& event,
                                           const ValidationContext& ctx) {
  auto start = std::chrono::steady_clock::now();
  ValidationResult result;

  const EventSchema* schema = nullptr;
  if (ctx.schemas) {
    schema = ctx.schemas->find(event.type);
  }

  if (!schema) {
    if (ctx.policy.reject_unknown_schema) {
      result.addIssue(ValidationIssueCode::UnknownSchema,
                      ValidationSeverity::Error, "type",
                      "No schema registered for event type");
    }
    auto end = std::chrono::steady_clock::now();
    result.duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    bool hasError = false;
    for (const auto& issue : result.issues) {
      if (issue.severity == ValidationSeverity::Error ||
          issue.severity == ValidationSeverity::Fatal) {
        hasError = true;
        break;
      }
    }
    result.status = hasError ? ValidationStatus::Invalid
                             : (result.issues.empty() ? ValidationStatus::Valid
                                                       : ValidationStatus::ValidWithWarnings);
    return result;
  }

  if (schema->version != event.provenance.schema_version &&
      event.provenance.schema_version > schema->version) {
    result.addIssue(ValidationIssueCode::UnsupportedSchemaVersion,
                    ValidationSeverity::Warning, "provenance.schema_version",
                    "Event schema version is newer than registered");
  }

  for (const auto& field : schema->required) {
    if (field.name == "path" && !event.hasPayload()) {
      result.addIssue(ValidationIssueCode::MissingRequiredField,
                      ValidationSeverity::Error, field.name,
                      "Required field missing from payload");
    }
  }

  if (ctx.policy.unknown_fields == DropPolicy::Reject) {
    if (event.hasPayload()) {
      for (const auto& [key, value] : event.payload.entries) {
        if (!schema->hasField(key)) {
          result.addIssue(ValidationIssueCode::UnknownField,
                          ValidationSeverity::Warning, key,
                          "Unknown field in payload");
        }
      }
    }
  } else if (ctx.policy.unknown_fields == DropPolicy::Warn) {
    if (event.hasPayload()) {
      for (const auto& [key, value] : event.payload.entries) {
        if (!schema->hasField(key)) {
          result.addIssue(ValidationIssueCode::UnknownField,
                          ValidationSeverity::Notice, key,
                          "Unknown field in payload");
        }
      }
    }
  }

  auto end = std::chrono::steady_clock::now();
  result.duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

  bool hasError = false;
  for (const auto& issue : result.issues) {
    if (issue.severity == ValidationSeverity::Error ||
        issue.severity == ValidationSeverity::Fatal) {
      hasError = true;
      break;
    }
  }
  result.status = hasError ? ValidationStatus::Invalid
                           : (result.issues.empty() ? ValidationStatus::Valid
                                                     : ValidationStatus::ValidWithWarnings);
  return result;
}

}  // namespace monix::validation
