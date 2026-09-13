#include "EventSchema.hpp"

namespace monix::validation {

const char* FieldTypeName(FieldType t) {
  switch (t) {
    case FieldType::String:   return "String";
    case FieldType::Integer:  return "Integer";
    case FieldType::Unsigned: return "Unsigned";
    case FieldType::Double:   return "Double";
    case FieldType::Boolean:  return "Boolean";
    case FieldType::Bytes:    return "Bytes";
    case FieldType::Any:      return "Any";
  }
  return "Unknown";
}

bool EventSchema::isRequired(const std::string& fieldName) const {
  for (const auto& f : required) {
    if (f.name == fieldName) return true;
  }
  return false;
}

bool EventSchema::isOptional(const std::string& fieldName) const {
  for (const auto& f : optional) {
    if (f.name == fieldName) return true;
  }
  return false;
}

bool EventSchema::hasField(const std::string& fieldName) const {
  return isRequired(fieldName) || isOptional(fieldName);
}

const FieldDefinition* EventSchema::findField(const std::string& fieldName) const {
  for (const auto& f : required) {
    if (f.name == fieldName) return &f;
  }
  for (const auto& f : optional) {
    if (f.name == fieldName) return &f;
  }
  return nullptr;
}

}  // namespace monix::validation
