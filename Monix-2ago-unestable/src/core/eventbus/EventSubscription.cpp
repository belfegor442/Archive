#include "EventSubscription.hpp"

namespace monix::eventbus {

EventSubscription::EventSubscription(SubscriptionId id, std::function<void()> cancelFn)
  : id_(id), cancelFn_(std::move(cancelFn)) {}

void EventSubscription::cancel() {
  if (cancelFn_) {
    cancelFn_();
    cancelFn_ = nullptr;
    id_ = 0;
  }
}

}  // namespace monix::eventbus
