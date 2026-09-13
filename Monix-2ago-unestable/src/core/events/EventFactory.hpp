#pragma once

#include "Event.hpp"
#include "SourceRef.hpp"
#include "ActorRef.hpp"
#include "EntityRef.hpp"
#include "Action.hpp"
#include "Correlation.hpp"
#include "Provenance.hpp"
#include "EventPayload.hpp"

#include <optional>

namespace monix::events {

class EventFactory {
public:
  static Event createSimple(
    const std::string& ns, const std::string& name,
    const SourceRef& source,
    EventSeverity severity = EventSeverity::Info);

  static Event createWithPayload(
    const std::string& ns, const std::string& name,
    const SourceRef& source,
    EventPayload payload,
    EventSeverity severity = EventSeverity::Info);

  static Event createFull(
    const std::string& ns, const std::string& name,
    const SourceRef& source,
    const Provenance& provenance,
    EventSeverity severity = EventSeverity::Info,
    std::optional<ActorRef> actor = std::nullopt,
    std::optional<EntityRef> entity = std::nullopt,
    std::optional<Action> action = std::nullopt,
    EventPayload payload = EventPayload{});

  static Event createDerived(
    const std::string& ns, const std::string& name,
    const SourceRef& source,
    const Provenance& provenance,
    const EventId& parentEventId,
    EventSeverity severity = EventSeverity::Info);
};

}  // namespace monix::events
