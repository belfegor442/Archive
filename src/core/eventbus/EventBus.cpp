#include "EventBus.hpp"

#include <algorithm>
#include <limits>

namespace monix::eventbus {

EventBus::EventBus(const EventBusConfig& config)
  : config_(config), queue_(config) {}

EventBus::~EventBus() {
  if (state_.load() == EventBusState::Running ||
      state_.load() == EventBusState::Draining) {
    shutdown();
  }
}

void EventBus::start() {
  auto expected = EventBusState::Created;
  if (!state_.compare_exchange_strong(expected, EventBusState::Running)) {
    auto expected2 = EventBusState::Stopped;
    if (!state_.compare_exchange_strong(expected2, EventBusState::Running)) return;
  }
  queue_.reopen();
  stopFlag_.store(false);
  for (std::size_t i = 0; i < config_.consumer_worker_count; ++i) {
    dispatcherThreads_.emplace_back(&EventBus::dispatcherLoop, this);
  }
}

void EventBus::stop() {
  auto expected = EventBusState::Running;
  if (!state_.compare_exchange_strong(expected, EventBusState::Stopped)) return;
  stopFlag_.store(true);
  queue_.close();
  for (auto& t : dispatcherThreads_) {
    if (t.joinable()) t.join();
  }
  dispatcherThreads_.clear();
}

void EventBus::drain() {
  auto expected = EventBusState::Running;
  if (!state_.compare_exchange_strong(expected, EventBusState::Draining)) return;
  queue_.close();
  auto deadline = std::chrono::steady_clock::now() + config_.shutdown_timeout;
  while (!queue_.empty() && std::chrono::steady_clock::now() < deadline) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  stopFlag_.store(true);
  for (auto& t : dispatcherThreads_) {
    if (t.joinable()) t.join();
  }
  dispatcherThreads_.clear();
  state_.store(EventBusState::Stopped);
}

void EventBus::shutdown() {
  drain();
}

IngestionResult EventBus::publish(events::Event event) {
  return publish(std::move(event), "");
}

IngestionResult EventBus::publish(events::Event event, ProducerId producerId) {
  if (state_.load() != EventBusState::Running) return IngestionResult::Shutdown;
  if (!event.id.isValid()) return IngestionResult::Invalid;

  BusEvent busEvent;
  busEvent.event = std::move(event);
  busEvent.priority = defaultPriorityForSeverity(busEvent.event.severity);
  busEvent.queued_at = events::currentSystemTimeMs();
  busEvent.producer_id = std::move(producerId);
  busEvent.queue_sequence = nextSequence_.fetch_add(1, std::memory_order_relaxed);

  std::string src = busEvent.event.source.id;
  metrics_.recordReceived(src, busEvent.queued_at);

  const EventPriority priority = busEvent.priority;
  bool accepted = queue_.pushWithPriority(std::move(busEvent), priority);
  if (accepted) {
    metrics_.recordAccepted(src, busEvent.queued_at);
    return IngestionResult::Accepted;
  }

  metrics_.recordDropped(src, "queue_overflow", busEvent.queued_at);
  return IngestionResult::QueueFull;
}

EventSubscription EventBus::subscribe(const EventFilter& filter, std::shared_ptr<IEventConsumer> consumer) {
  SubscriptionId id = nextSubId_.fetch_add(1, std::memory_order_relaxed);
  std::lock_guard<std::mutex> lock(subMu_);
  subscriptions_.push_back({id, filter, std::move(consumer)});
  return EventSubscription(id, [this, id]() { unsubscribe(id); });
}

void EventBus::unsubscribe(SubscriptionId id) {
  std::lock_guard<std::mutex> lock(subMu_);
  subscriptions_.erase(
    std::remove_if(subscriptions_.begin(), subscriptions_.end(),
      [id](const SubscriptionEntry& e) { return e.id == id; }),
    subscriptions_.end());
}

void EventBus::dispatcherLoop() {
  while (!stopFlag_.load()) {
    BusEvent busEvent;
    if (!queue_.pop(busEvent)) {
      if (stopFlag_.load()) break;
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
      continue;
    }
    metrics_.updateQueueDepth(queue_.size());
    dispatchToSubscribers(busEvent.event);
  }
  BusEvent remaining;
  while (queue_.pop(remaining)) {
    metrics_.updateQueueDepth(queue_.size());
    dispatchToSubscribers(remaining.event);
  }
}

void EventBus::dispatchToSubscribers(const events::Event& event) {
  std::vector<SubscriptionEntry> subscriptions;
  {
    std::lock_guard<std::mutex> lock(subMu_);
    subscriptions = subscriptions_;
  }
  for (auto& entry : subscriptions) {
    dispatchToConsumer(event, entry);
  }
}

void EventBus::dispatchToConsumer(const events::Event& event, SubscriptionEntry& entry) {
  if (!entry.filter.empty()) {
    if (!entry.filter.matches(event.type)) return;
    if (!entry.filter.matches(event.source.id)) return;
    if (!entry.filter.matches(event.severity)) return;
  }
  if (!entry.consumer) return;

  ConsumeResult result = ConsumeResult::Failed;
  for (std::size_t attempt = 0; attempt <= config_.max_retries; ++attempt) {
    result = entry.consumer->consume(event);
    if (result == ConsumeResult::Accepted) return;
    if (result == ConsumeResult::Rejected) return;
    if (result == ConsumeResult::Retry) {
      metrics_.recordRetry();
      if (attempt < config_.max_retries) {
        std::this_thread::sleep_for(config_.retry_delay);
        continue;
      }
    }
    break;
  }

  metrics_.recordConsumerFailure();
  DeadLetterEntry dle;
  dle.event_id = event.id;
  dle.consumer_id = std::to_string(entry.id);
  dle.failure_reason = ConsumeResultName(result);
  dle.retry_count = static_cast<std::uint32_t>(std::min<std::size_t>(
    config_.max_retries, std::numeric_limits<std::uint32_t>::max()));
  dle.first_failure = events::currentSystemTimeMs();
  dle.last_failure = events::currentSystemTimeMs();
  deadLetters_.push(std::move(dle));
}

EventBusHealth EventBus::health() const {
  EventBusHealth h;
  h.state = state_.load();
  auto m = metrics_.snapshot();
  h.received = m.events_received;
  h.accepted = m.events_accepted;
  h.rejected = m.events_rejected;
  h.dropped = m.events_dropped;
  h.queue_depth = m.queue_depth;
  h.queue_capacity = config_.queue_capacity;
  h.pressure = queue_.pressure();
  h.ingestion_rate = m.ingestion_rate;
  h.dispatch_rate = m.dispatch_rate;
  h.consumer_failures = m.consumer_failures;
  return h;
}

}  // namespace monix::eventbus
