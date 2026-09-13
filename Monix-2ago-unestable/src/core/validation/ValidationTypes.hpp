#pragma once

#include <cstdint>
#include <string>
#include <chrono>
#include <vector>

namespace monix::validation {

enum class ValidationStatus : std::uint8_t {
  Valid,
  ValidWithWarnings,
  Invalid
};

const char* ValidationStatusName(ValidationStatus s);
ValidationStatus ValidationStatusFromInt(int v);

enum class ValidationSeverity : std::uint8_t {
  Notice,
  Warning,
  Error,
  Fatal
};

const char* ValidationSeverityName(ValidationSeverity s);

enum class ValidationIssueCode : std::uint16_t {
  None = 0,
  MissingEventId,
  MissingEventType,
  MissingTimestamp,
  MissingSource,
  MissingProvenance,
  InvalidEventId,
  InvalidType,
  InvalidTimestamp,
  InvalidSource,
  InvalidActor,
  InvalidEntity,
  InvalidAction,
  InvalidProvenance,
  InvalidCorrelation,
  PayloadTooLarge,
  InvalidEncoding,
  UnknownSchema,
  UnsupportedSchemaVersion,
  UnknownField,
  MissingRequiredField,
  InvalidFieldValue,
  InvalidFieldType,
  SemanticConflict,
  ClockSkewDetected,
  FutureTimestamp,
  SequenceGap,
  DuplicateEventId,
  IntegrityFailure,
  IntegrityNotComputed,
  SchemaVersionMismatch,
  SourceNotRegistered,
  ActorEntityMismatch,
  ActionTypeMismatch
};

const char* ValidationIssueCodeName(ValidationIssueCode c);

enum class ValidationProfile : std::uint8_t {
  Fast,
  Standard,
  Strict,
  Forensic
};

const char* ValidationProfileName(ValidationProfile p);

enum class DropPolicy : std::uint8_t {
  Allow,
  Warn,
  Reject
};

struct ValidationIssue {
  ValidationIssueCode code = ValidationIssueCode::None;
  ValidationSeverity severity = ValidationSeverity::Notice;
  std::string field;
  std::string message;
};

}  // namespace monix::validation
