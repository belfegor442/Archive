#include "EventErrors.hpp"

namespace monix::events {

const char* EventErrorName(EventError err) {
  switch (err) {
    case EventError::None:                  return "None";
    case EventError::InvalidEventId:        return "InvalidEventId";
    case EventError::MissingEventType:      return "MissingEventType";
    case EventError::MissingTimestamp:       return "MissingTimestamp";
    case EventError::InvalidTimestamp:       return "InvalidTimestamp";
    case EventError::MissingSource:         return "MissingSource";
    case EventError::MissingProvenance:     return "MissingProvenance";
    case EventError::InvalidSchema:         return "InvalidSchema";
    case EventError::PayloadTooLarge:       return "PayloadTooLarge";
    case EventError::MetadataTooLarge:      return "MetadataTooLarge";
    case EventError::InvalidPayload:        return "InvalidPayload";
    case EventError::InvalidCorrelation:    return "InvalidCorrelation";
    case EventError::SerializationFailed:   return "SerializationFailed";
    case EventError::DeserializationFailed: return "DeserializationFailed";
    case EventError::MissingRequiredField:  return "MissingRequiredField";
    case EventError::InvalidFieldFormat:    return "InvalidFieldFormat";
    case EventError::DuplicateMetadataKey:  return "DuplicateMetadataKey";
    case EventError::EventTooLarge:         return "EventTooLarge";
  }
  return "Unknown";
}

const char* EventErrorDescription(EventError err) {
  switch (err) {
    case EventError::None:                  return "No error";
    case EventError::InvalidEventId:        return "Event ID is invalid or empty";
    case EventError::MissingEventType:      return "Event type namespace and name are required";
    case EventError::MissingTimestamp:       return "Occurrence and ingestion timestamps are required";
    case EventError::InvalidTimestamp:       return "Timestamp value is invalid";
    case EventError::MissingSource:         return "Source reference is required";
    case EventError::MissingProvenance:     return "Provenance is required";
    case EventError::InvalidSchema:         return "Schema name or version is invalid";
    case EventError::PayloadTooLarge:       return "Payload exceeds maximum size limit";
    case EventError::MetadataTooLarge:      return "Metadata exceeds maximum entries or value size";
    case EventError::InvalidPayload:        return "Payload contains invalid values";
    case EventError::InvalidCorrelation:    return "Correlation contains invalid references";
    case EventError::SerializationFailed:   return "Failed to serialize event";
    case EventError::DeserializationFailed: return "Failed to deserialize event";
    case EventError::MissingRequiredField:  return "A required field is missing";
    case EventError::InvalidFieldFormat:    return "Field format is incorrect";
    case EventError::DuplicateMetadataKey:  return "Duplicate metadata key detected";
    case EventError::EventTooLarge:         return "Total event size exceeds limit";
  }
  return "Unknown error";
}

}  // namespace monix::events
