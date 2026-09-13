#pragma once

#include "EventFilter.hpp"
#include "../events/Event.hpp"
#include "../events/EventTime.hpp"

#include <cstdint>
#include <string>
#include <chrono>

namespace monix::eventbus {

using ProducerId = std::string;
using ConsumerId = std::string;

struct ProducerInfo {
  ProducerId id;
  std::string name;
  std::string version;
};

struct BusEvent {
  events::Event event;
  EventPriority priority = EventPriority::Normal;
  events::Timestamp queued_at = 0;
  ProducerId producer_id;
  std::uint64_t queue_sequence = 0;
};

}  // namespace monix::eventbus
