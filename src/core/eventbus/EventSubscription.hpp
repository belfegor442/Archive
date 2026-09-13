#pragma once

#include "EventBusErrors.hpp"
#include "../events/Event.hpp"

#include <cstdint>
#include <functional>

namespace monix::eventbus {

using SubscriptionId = std::uint64_t;

class IEventConsumer {
public:
  virtual ~IEventConsumer() = default;
  virtual ConsumeResult consume(const events::Event& event) = 0;
};

class EventSubscription {
public:
  EventSubscription() = default;
  EventSubscription(SubscriptionId id, std::function<void()> cancelFn);

  SubscriptionId id() const { return id_; }
  bool isValid() const { return id_ != 0 && cancelFn_ != nullptr; }
  void cancel();

private:
  SubscriptionId id_ = 0;
  std::function<void()> cancelFn_;
};

}  // namespace monix::eventbus
