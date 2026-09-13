#include "EventBusErrors.hpp"

namespace monix::eventbus {

const char* EventBusErrorName(EventBusError err) {
  switch (err) {
    case EventBusError::None:               return "None";
    case EventBusError::QueueFull:          return "QueueFull";
    case EventBusError::QueueClosed:        return "QueueClosed";
    case EventBusError::InvalidEvent:       return "InvalidEvent";
    case EventBusError::ConsumerFailed:     return "ConsumerFailed";
    case EventBusError::ConsumerTimeout:    return "ConsumerTimeout";
    case EventBusError::DispatchFailed:     return "DispatchFailed";
    case EventBusError::ShutdownTimeout:    return "ShutdownTimeout";
    case EventBusError::AlreadySubscribed:  return "AlreadySubscribed";
    case EventBusError::SubscriptionNotFound: return "SubscriptionNotFound";
    case EventBusError::BusNotRunning:      return "BusNotRunning";
    case EventBusError::BusAlreadyStarted:  return "BusAlreadyStarted";
  }
  return "Unknown";
}

const char* EventBusErrorDescription(EventBusError err) {
  switch (err) {
    case EventBusError::None:               return "No error";
    case EventBusError::QueueFull:          return "Event queue is at capacity";
    case EventBusError::QueueClosed:        return "Event queue is closed";
    case EventBusError::InvalidEvent:       return "Event failed validation";
    case EventBusError::ConsumerFailed:     return "Consumer returned failure";
    case EventBusError::ConsumerTimeout:    return "Consumer timed out";
    case EventBusError::DispatchFailed:     return "Failed to dispatch event";
    case EventBusError::ShutdownTimeout:    return "Shutdown timed out";
    case EventBusError::AlreadySubscribed:  return "Subscription already exists";
    case EventBusError::SubscriptionNotFound: return "Subscription not found";
    case EventBusError::BusNotRunning:      return "Event bus is not running";
    case EventBusError::BusAlreadyStarted:  return "Event bus already started";
  }
  return "Unknown error";
}

const char* IngestionResultName(IngestionResult r) {
  switch (r) {
    case IngestionResult::Accepted:    return "Accepted";
    case IngestionResult::QueueFull:   return "QueueFull";
    case IngestionResult::Rejected:    return "Rejected";
    case IngestionResult::Invalid:     return "Invalid";
    case IngestionResult::Shutdown:    return "Shutdown";
    case IngestionResult::RateLimited: return "RateLimited";
  }
  return "Unknown";
}

const char* ConsumeResultName(ConsumeResult r) {
  switch (r) {
    case ConsumeResult::Accepted: return "Accepted";
    case ConsumeResult::Retry:    return "Retry";
    case ConsumeResult::Rejected: return "Rejected";
    case ConsumeResult::Failed:   return "Failed";
  }
  return "Unknown";
}

const char* EventBusStateName(EventBusState s) {
  switch (s) {
    case EventBusState::Created:  return "Created";
    case EventBusState::Running:  return "Running";
    case EventBusState::Draining: return "Draining";
    case EventBusState::Stopped:  return "Stopped";
    case EventBusState::Failed:   return "Failed";
  }
  return "Unknown";
}

}  // namespace monix::eventbus
