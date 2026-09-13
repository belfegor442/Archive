#pragma once

#include "EventBusErrors.hpp"
#include "EventBusMetrics.hpp"

#include <cstdint>

namespace monix::eventbus {

struct EventBusHealth {
  EventBusState state = EventBusState::Created;
  std::uint64_t received = 0;
  std::uint64_t accepted = 0;
  std::uint64_t rejected = 0;
  std::uint64_t dropped = 0;
  std::uint64_t queue_depth = 0;
  std::uint64_t queue_capacity = 0;
  QueuePressure pressure = QueuePressure::Normal;
  double ingestion_rate = 0.0;
  double dispatch_rate = 0.0;
  std::uint64_t consumer_failures = 0;

  double dropRate() const {
    return received > 0 ? static_cast<double>(dropped) / static_cast<double>(received) : 0.0;
  }
};

}  // namespace monix::eventbus
