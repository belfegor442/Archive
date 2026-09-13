#pragma once

#include "../events/EventType.hpp"
#include "../events/EventPayload.hpp"

#include <cstdint>
#include <string>
#include <vector>
#include <functional>

namespace monix::validation {

enum class FieldType : std::uint8_t {
  String,
  Integer,
  Unsigned,
  Double,
  Boolean,
  Bytes,
  Any
};

const char* FieldTypeName(FieldType t);

struct FieldDefinition {
  std::string name;
  FieldType type = FieldType::Any;
  bool required = false;
  std::string description;
};

enum class RuleKind : std::uint8_t {
  Range,
  Regex,
  Custom
};

struct ValidationRule {
  std::string field;
  RuleKind kind = RuleKind::Custom;
  std::string expression;
  std::string description;
};

struct ValidationRules {
  std::vector<ValidationRule> rules;
};

struct EventSchema {
  std::string name;
  std::uint32_t version = 1;
  events::EventType type;
  std::vector<FieldDefinition> required;
  std::vector<FieldDefinition> optional;
  ValidationRules rules;

  bool isRequired(const std::string& fieldName) const;
  bool isOptional(const std::string& fieldName) const;
  bool hasField(const std::string& fieldName) const;
  const FieldDefinition* findField(const std::string& fieldName) const;
};

}  // namespace monix::validation
