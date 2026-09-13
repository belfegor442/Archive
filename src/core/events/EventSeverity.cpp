#include "EventSeverity.hpp"

#include <cstring>

namespace monix::events {

const char* EventSeverityName(EventSeverity s) {
  switch (s) {
    case EventSeverity::Trace:    return "trace";
    case EventSeverity::Debug:    return "debug";
    case EventSeverity::Info:     return "info";
    case EventSeverity::Notice:   return "notice";
    case EventSeverity::Warning:  return "warning";
    case EventSeverity::Error:    return "error";
    case EventSeverity::Critical: return "critical";
  }
  return "info";
}

EventSeverity EventSeverityFromString(const char* s) {
  if (std::strcmp(s, "trace") == 0)    return EventSeverity::Trace;
  if (std::strcmp(s, "debug") == 0)    return EventSeverity::Debug;
  if (std::strcmp(s, "info") == 0)     return EventSeverity::Info;
  if (std::strcmp(s, "notice") == 0)   return EventSeverity::Notice;
  if (std::strcmp(s, "warning") == 0)  return EventSeverity::Warning;
  if (std::strcmp(s, "error") == 0)    return EventSeverity::Error;
  if (std::strcmp(s, "critical") == 0) return EventSeverity::Critical;
  return EventSeverity::Info;
}

}  // namespace monix::events
