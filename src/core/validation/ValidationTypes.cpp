#include "ValidationTypes.hpp"

namespace monix::validation {

const char* ValidationStatusName(ValidationStatus s) {
  switch (s) {
    case ValidationStatus::Valid:             return "Valid";
    case ValidationStatus::ValidWithWarnings: return "ValidWithWarnings";
    case ValidationStatus::Invalid:           return "Invalid";
  }
  return "Unknown";
}

ValidationStatus ValidationStatusFromInt(int v) {
  switch (v) {
    case 0: return ValidationStatus::Valid;
    case 1: return ValidationStatus::ValidWithWarnings;
    case 2: return ValidationStatus::Invalid;
  }
  return ValidationStatus::Invalid;
}

const char* ValidationSeverityName(ValidationSeverity s) {
  switch (s) {
    case ValidationSeverity::Notice:  return "Notice";
    case ValidationSeverity::Warning: return "Warning";
    case ValidationSeverity::Error:   return "Error";
    case ValidationSeverity::Fatal:   return "Fatal";
  }
  return "Unknown";
}

const char* ValidationIssueCodeName(ValidationIssueCode c) {
  switch (c) {
    case ValidationIssueCode::None:                  return "None";
    case ValidationIssueCode::MissingEventId:        return "MissingEventId";
    case ValidationIssueCode::MissingEventType:      return "MissingEventType";
    case ValidationIssueCode::MissingTimestamp:      return "MissingTimestamp";
    case ValidationIssueCode::MissingSource:         return "MissingSource";
    case ValidationIssueCode::MissingProvenance:     return "MissingProvenance";
    case ValidationIssueCode::InvalidEventId:        return "InvalidEventId";
    case ValidationIssueCode::InvalidType:           return "InvalidType";
    case ValidationIssueCode::InvalidTimestamp:      return "InvalidTimestamp";
    case ValidationIssueCode::InvalidSource:         return "InvalidSource";
    case ValidationIssueCode::InvalidActor:          return "InvalidActor";
    case ValidationIssueCode::InvalidEntity:         return "InvalidEntity";
    case ValidationIssueCode::InvalidAction:         return "InvalidAction";
    case ValidationIssueCode::InvalidProvenance:     return "InvalidProvenance";
    case ValidationIssueCode::InvalidCorrelation:    return "InvalidCorrelation";
    case ValidationIssueCode::PayloadTooLarge:       return "PayloadTooLarge";
    case ValidationIssueCode::InvalidEncoding:       return "InvalidEncoding";
    case ValidationIssueCode::UnknownSchema:         return "UnknownSchema";
    case ValidationIssueCode::UnsupportedSchemaVersion: return "UnsupportedSchemaVersion";
    case ValidationIssueCode::UnknownField:          return "UnknownField";
    case ValidationIssueCode::MissingRequiredField:  return "MissingRequiredField";
    case ValidationIssueCode::InvalidFieldValue:     return "InvalidFieldValue";
    case ValidationIssueCode::InvalidFieldType:      return "InvalidFieldType";
    case ValidationIssueCode::SemanticConflict:      return "SemanticConflict";
    case ValidationIssueCode::ClockSkewDetected:     return "ClockSkewDetected";
    case ValidationIssueCode::FutureTimestamp:        return "FutureTimestamp";
    case ValidationIssueCode::SequenceGap:           return "SequenceGap";
    case ValidationIssueCode::DuplicateEventId:      return "DuplicateEventId";
    case ValidationIssueCode::IntegrityFailure:      return "IntegrityFailure";
    case ValidationIssueCode::IntegrityNotComputed:  return "IntegrityNotComputed";
    case ValidationIssueCode::SchemaVersionMismatch: return "SchemaVersionMismatch";
    case ValidationIssueCode::SourceNotRegistered:   return "SourceNotRegistered";
    case ValidationIssueCode::ActorEntityMismatch:   return "ActorEntityMismatch";
    case ValidationIssueCode::ActionTypeMismatch:    return "ActionTypeMismatch";
    default: return "Unknown";
  }
  return "Unknown";
}

const char* ValidationProfileName(ValidationProfile p) {
  switch (p) {
    case ValidationProfile::Fast:      return "Fast";
    case ValidationProfile::Standard:  return "Standard";
    case ValidationProfile::Strict:    return "Strict";
    case ValidationProfile::Forensic:  return "Forensic";
  }
  return "Unknown";
}

}  // namespace monix::validation
