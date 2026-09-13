#pragma once

#include <cstdint>

namespace monix::events {

enum class EventSeverity : std::uint8_t {
  Trace    = 0,
  Debug    = 1,
  Info     = 2,
  Notice   = 3,
  Warning  = 4,
  Error    = 5,
  Critical = 6
};

const char* EventSeverityName(EventSeverity s);
EventSeverity EventSeverityFromString(const char* s);

}  // namespace monix::events
