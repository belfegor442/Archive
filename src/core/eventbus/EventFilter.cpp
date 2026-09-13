#include "EventFilter.hpp"

#include <algorithm>

namespace monix::eventbus {

bool EventFilter::matches(const events::EventType& t) const {
  if (types.empty()) return true;
  for (const auto& ft : types) {
    if (ft == t) return true;
    if (!ft.namespace_name.empty() && ft.namespace_name == t.namespace_name && ft.name.empty()) return true;
  }
  return false;
}

bool EventFilter::matches(const events::SourceId& s) const {
  if (sources.empty()) return true;
  for (const auto& fs : sources) {
    if (fs == s) return true;
  }
  return false;
}

bool EventFilter::matches(events::EventSeverity sev) const {
  if (severities.empty()) return true;
  for (const auto& fs : severities) {
    if (fs == sev) return true;
  }
  return false;
}

bool EventFilter::matches(EventPriority p) const {
  if (priorities.empty()) return true;
  for (const auto& fp : priorities) {
    if (fp == p) return true;
  }
  return false;
}

EventPriority defaultPriorityForSeverity(events::EventSeverity sev) {
  switch (sev) {
    case events::EventSeverity::Critical: return EventPriority::Critical;
    case events::EventSeverity::Error:    return EventPriority::High;
    case events::EventSeverity::Warning:  return EventPriority::Elevated;
    case events::EventSeverity::Notice:   return EventPriority::Normal;
    case events::EventSeverity::Info:     return EventPriority::Normal;
    case events::EventSeverity::Debug:    return EventPriority::Background;
    case events::EventSeverity::Trace:    return EventPriority::Background;
  }
  return EventPriority::Normal;
}

}  // namespace monix::eventbus
