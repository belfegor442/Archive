#pragma once

#include <cstdint>
#include <string>

namespace monix::events {

enum class EventError : std::uint32_t {
  None = 0,
  InvalidEventId,
  MissingEventType,
  MissingTimestamp,
  InvalidTimestamp,
  MissingSource,
  MissingProvenance,
  InvalidSchema,
  PayloadTooLarge,
  MetadataTooLarge,
  InvalidPayload,
  InvalidCorrelation,
  SerializationFailed,
  DeserializationFailed,
  MissingRequiredField,
  InvalidFieldFormat,
  DuplicateMetadataKey,
  EventTooLarge
};

const char* EventErrorName(EventError err);
const char* EventErrorDescription(EventError err);

struct EventErrorDetail {
  EventError code = EventError::None;
  std::string message;
  std::string field;

  EventErrorDetail() = default;
  EventErrorDetail(EventError c, std::string msg, std::string fld = "")
    : code(c), message(std::move(msg)), field(std::move(fld)) {}

  bool hasError() const { return code != EventError::None; }
};

}  // namespace monix::events
