#pragma once

#include "EventId.hpp"
#include "EventTime.hpp"
#include "EventType.hpp"
#include "EventSeverity.hpp"
#include "EventFlags.hpp"
#include "SourceRef.hpp"
#include "ActorRef.hpp"
#include "EntityRef.hpp"
#include "Action.hpp"
#include "Correlation.hpp"
#include "Provenance.hpp"
#include "EventPayload.hpp"
#include "EventErrors.hpp"

#include <cstdint>
#include <optional>

namespace monix::events {

using EventSequence = std::uint64_t;

struct Event {
  static constexpr std::size_t kMaxPayloadSize = 65536;
  static constexpr std::size_t kMaxMetadataEntries = 64;
  static constexpr std::size_t kMaxMetadataValueSize = 4096;
  static constexpr std::uint32_t kCurrentSchemaVersion = 1;

  EventId id;
  EventSequence sequence = 0;

  EventTime time;
  std::optional<MonotonicTime> monotonic_time;

  EventType type;
  EventSeverity severity = EventSeverity::Info;

  SourceRef source;

  std::optional<ActorRef> actor;
  std::optional<EntityRef> entity;

  std::optional<Action> action;

  Correlation correlation;

  Provenance provenance;

  EventFlags flags = EventFlags::None;

  EventMetadata metadata;
  EventPayload payload;

  std::string qualifiedType() const { return type.qualifiedName(); }
  bool hasActor() const { return actor.has_value(); }
  bool hasEntity() const { return entity.has_value(); }
  bool hasCorrelation() const { return correlation.hasAny(); }
  bool hasPayload() const { return !payload.empty(); }
  bool hasMetadata() const { return !metadata.empty(); }
  bool isRaw() const { return !hasFlag(flags, EventFlags::Derived); }
  bool isDerived() const { return hasFlag(flags, EventFlags::Derived); }
};

}  // namespace monix::events
