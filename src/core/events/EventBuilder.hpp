#pragma once

#include "Event.hpp"
#include "EventErrors.hpp"

#include <cstdint>
#include <string>
#include <optional>
#include <variant>

namespace monix::events {

class EventBuilder {
public:
  EventBuilder();

  EventBuilder& type(EventType t);
  EventBuilder& type(const std::string& ns, const std::string& name, std::uint32_t numeric_id = 0);
  EventBuilder& severity(EventSeverity s);
  EventBuilder& source(SourceRef s);
  EventBuilder& occurrenceTime(Timestamp t);
  EventBuilder& ingestionTime(Timestamp t);
  EventBuilder& monotonicTime(MonotonicTime t);
  EventBuilder& sequence(EventSequence seq);
  EventBuilder& actor(ActorRef a);
  EventBuilder& entity(EntityRef e);
  EventBuilder& action(Action a);
  EventBuilder& action(const std::string& name);
  EventBuilder& correlation(Correlation c);
  EventBuilder& provenance(Provenance p);
  EventBuilder& flags(EventFlags f);
  EventBuilder& payload(EventPayload p);
  EventBuilder& metadata(EventMetadata m);
  EventBuilder& metadata(const std::string& key, PayloadValue value);

  std::variant<Event, EventErrorDetail> build();

  void reset();

private:
  Event event_;
  bool built_ = false;
};

}  // namespace monix::events
