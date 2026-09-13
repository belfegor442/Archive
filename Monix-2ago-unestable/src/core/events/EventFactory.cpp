#include "EventFactory.hpp"
#include "EventBuilder.hpp"

namespace monix::events {

Event EventFactory::createSimple(
    const std::string& ns, const std::string& name,
    const SourceRef& source,
    EventSeverity severity) {
  auto result = EventBuilder()
    .type(ns, name)
    .severity(severity)
    .source(source)
    .provenance(Provenance{
      source.id,
      source.name,
      source.version,
      "monix.event",
      Event::kCurrentSchemaVersion,
      ProvenanceKind::NativeObservation
    })
    .build();
  return std::get<Event>(std::move(result));
}

Event EventFactory::createWithPayload(
    const std::string& ns, const std::string& name,
    const SourceRef& source,
    EventPayload payload,
    EventSeverity severity) {
  auto result = EventBuilder()
    .type(ns, name)
    .severity(severity)
    .source(source)
    .payload(std::move(payload))
    .provenance(Provenance{
      source.id,
      source.name,
      source.version,
      "monix.event",
      Event::kCurrentSchemaVersion,
      ProvenanceKind::NativeObservation
    })
    .build();
  return std::get<Event>(std::move(result));
}

Event EventFactory::createFull(
    const std::string& ns, const std::string& name,
    const SourceRef& source,
    const Provenance& provenance,
    EventSeverity severity,
    std::optional<ActorRef> actor,
    std::optional<EntityRef> entity,
    std::optional<Action> action,
    EventPayload payload) {
  EventBuilder builder;
  builder.type(ns, name)
    .severity(severity)
    .source(source)
    .provenance(provenance)
    .payload(std::move(payload));
  if (actor) builder.actor(std::move(*actor));
  if (entity) builder.entity(std::move(*entity));
  if (action) builder.action(std::move(*action));
  auto result = builder.build();
  return std::get<Event>(std::move(result));
}

Event EventFactory::createDerived(
    const std::string& ns, const std::string& name,
    const SourceRef& source,
    const Provenance& provenance,
    const EventId& parentEventId,
    EventSeverity severity) {
  Correlation corr;
  corr.parent_event_id = parentEventId;

  Provenance derivedProv = provenance;
  derivedProv.kind = ProvenanceKind::Derived;

  auto result = EventBuilder()
    .type(ns, name)
    .severity(severity)
    .source(source)
    .provenance(derivedProv)
    .correlation(std::move(corr))
    .flags(EventFlags::Derived)
    .build();
  return std::get<Event>(std::move(result));
}

}  // namespace monix::events
