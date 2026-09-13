#pragma once

#include "../events/EventType.hpp"
#include "../events/EventSeverity.hpp"
#include "../events/SourceRef.hpp"

#include <vector>
#include <string>

namespace monix::eventbus {

enum class EventPriority : std::uint8_t {
  Background = 0,
  Normal     = 1,
  Elevated   = 2,
  High       = 3,
  Critical   = 4
};

struct EventFilter {
  std::vector<events::EventType> types;
  std::vector<events::SourceId> sources;
  std::vector<events::EventSeverity> severities;
  std::vector<EventPriority> priorities;

  bool empty() const {
    return types.empty() && sources.empty() && severities.empty() && priorities.empty();
  }

  bool matches(const events::EventType& t) const;
  bool matches(const events::SourceId& s) const;
  bool matches(events::EventSeverity sev) const;
  bool matches(EventPriority p) const;
};

EventPriority defaultPriorityForSeverity(events::EventSeverity sev);

}  // namespace monix::eventbus
