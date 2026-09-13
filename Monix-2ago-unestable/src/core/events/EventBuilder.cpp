#include "EventBuilder.hpp"

namespace monix::events {

EventBuilder::EventBuilder() = default;

EventBuilder& EventBuilder::type(EventType t) {
  event_.type = std::move(t);
  return *this;
}

EventBuilder& EventBuilder::type(const std::string& ns, const std::string& name, std::uint32_t numeric_id) {
  event_.type = EventType{ns, name, numeric_id};
  return *this;
}

EventBuilder& EventBuilder::severity(EventSeverity s) {
  event_.severity = s;
  return *this;
}

EventBuilder& EventBuilder::source(SourceRef s) {
  event_.source = std::move(s);
  return *this;
}

EventBuilder& EventBuilder::occurrenceTime(Timestamp t) {
  event_.time.occurrence = t;
  return *this;
}

EventBuilder& EventBuilder::ingestionTime(Timestamp t) {
  event_.time.ingestion = t;
  return *this;
}

EventBuilder& EventBuilder::monotonicTime(MonotonicTime t) {
  event_.monotonic_time = t;
  return *this;
}

EventBuilder& EventBuilder::sequence(EventSequence seq) {
  event_.sequence = seq;
  return *this;
}

EventBuilder& EventBuilder::actor(ActorRef a) {
  event_.actor = std::move(a);
  return *this;
}

EventBuilder& EventBuilder::entity(EntityRef e) {
  event_.entity = std::move(e);
  return *this;
}

EventBuilder& EventBuilder::action(Action a) {
  event_.action = std::move(a);
  return *this;
}

EventBuilder& EventBuilder::action(const std::string& name) {
  event_.action = Action{name, std::nullopt};
  return *this;
}

EventBuilder& EventBuilder::correlation(Correlation c) {
  event_.correlation = std::move(c);
  return *this;
}

EventBuilder& EventBuilder::provenance(Provenance p) {
  event_.provenance = std::move(p);
  return *this;
}

EventBuilder& EventBuilder::flags(EventFlags f) {
  event_.flags = f;
  return *this;
}

EventBuilder& EventBuilder::payload(EventPayload p) {
  event_.payload = std::move(p);
  return *this;
}

EventBuilder& EventBuilder::metadata(EventMetadata m) {
  event_.metadata = std::move(m);
  return *this;
}

EventBuilder& EventBuilder::metadata(const std::string& key, PayloadValue value) {
  event_.metadata.set(key, std::move(value));
  return *this;
}

std::variant<Event, EventErrorDetail> EventBuilder::build() {
  if (!event_.id.isValid()) {
    event_.id = EventId::generate();
  }
  if (!event_.id.isValid()) {
    return EventErrorDetail(EventError::InvalidEventId, "Failed to generate valid EventId");
  }
  if (!event_.type.isValid()) {
    return EventErrorDetail(EventError::MissingEventType, "Event type namespace and name are required");
  }
  if (!event_.time.isValid()) {
    if (event_.time.occurrence == 0) {
      event_.time.occurrence = currentSystemTimeMs();
    }
    if (event_.time.ingestion == 0) {
      event_.time.ingestion = currentSystemTimeMs();
    }
    if (!event_.time.isValid()) {
      return EventErrorDetail(EventError::MissingTimestamp, "Occurrence and ingestion timestamps are required");
    }
  }
  if (!event_.source.isValid()) {
    return EventErrorDetail(EventError::MissingSource, "Source reference is required");
  }
  if (!event_.provenance.isValid()) {
    return EventErrorDetail(EventError::MissingProvenance, "Provenance is required");
  }
  if (event_.metadata.size() > EventMetadata::kMaxEntries) {
    return EventErrorDetail(EventError::MetadataTooLarge, "Metadata exceeds maximum entries limit");
  }
  built_ = true;
  return std::move(event_);
}

void EventBuilder::reset() {
  event_ = Event{};
  built_ = false;
}

}  // namespace monix::events
