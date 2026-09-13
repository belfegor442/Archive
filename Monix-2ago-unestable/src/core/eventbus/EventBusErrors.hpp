#pragma once

#include <cstdint>
#include <string>

namespace monix::eventbus {

enum class EventBusError : std::uint32_t {
  None = 0,
  QueueFull,
  QueueClosed,
  InvalidEvent,
  ConsumerFailed,
  ConsumerTimeout,
  DispatchFailed,
  ShutdownTimeout,
  AlreadySubscribed,
  SubscriptionNotFound,
  BusNotRunning,
  BusAlreadyStarted
};

const char* EventBusErrorName(EventBusError err);
const char* EventBusErrorDescription(EventBusError err);

enum class IngestionResult : std::uint8_t {
  Accepted,
  QueueFull,
  Rejected,
  Invalid,
  Shutdown,
  RateLimited
};

const char* IngestionResultName(IngestionResult r);

enum class ConsumeResult : std::uint8_t {
  Accepted,
  Retry,
  Rejected,
  Failed
};

const char* ConsumeResultName(ConsumeResult r);

enum class EventBusState : std::uint8_t {
  Created,
  Running,
  Draining,
  Stopped,
  Failed
};

const char* EventBusStateName(EventBusState s);

}  // namespace monix::eventbus
