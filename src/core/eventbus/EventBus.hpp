#pragma once

#include "EventBusConfig.hpp"
#include "EventBusErrors.hpp"
#include "EventBusMetrics.hpp"
#include "EventBusHealth.hpp"
#include "EventFilter.hpp"
#include "EventSubscription.hpp"
#include "EventProducer.hpp"
#include "EventQueue.hpp"
#include "DeadLetterQueue.hpp"

#include "../events/Event.hpp"
#include "../events/EventTime.hpp"

#include <cstdint>
#include <vector>
#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <functional>

namespace monix::eventbus {

struct SubscriptionEntry {
  SubscriptionId id;
  EventFilter filter;
  std::shared_ptr<IEventConsumer> consumer;
};

class EventBus {
public:
  explicit EventBus(const EventBusConfig& config = EventBusConfig{});
  ~EventBus();

  EventBus(const EventBus&) = delete;
  EventBus& operator=(const EventBus&) = delete;

  void start();
  void stop();
  void drain();
  void shutdown();

  IngestionResult publish(events::Event event);
  IngestionResult publish(events::Event event, ProducerId producerId);

  EventSubscription subscribe(const EventFilter& filter, std::shared_ptr<IEventConsumer> consumer);
  void unsubscribe(SubscriptionId id);

  EventBusHealth health() const;
  EventBusState state() const { return state_.load(std::memory_order_relaxed); }
  const EventBusMetrics& metrics() const { return metrics_; }
  const DeadLetterQueue& deadLetters() const { return deadLetters_; }

private:
  void dispatcherLoop();
  void dispatchToSubscribers(const events::Event& event);
  void dispatchToConsumer(const events::Event& event, SubscriptionEntry& entry);

  EventBusConfig config_;
  std::atomic<EventBusState> state_{EventBusState::Created};
  std::atomic<SubscriptionId> nextSubId_{1};
  std::atomic<std::uint64_t> nextSequence_{0};

  EventQueue queue_;
  EventBusMetrics metrics_;
  DeadLetterQueue deadLetters_;

  std::vector<SubscriptionEntry> subscriptions_;
  std::mutex subMu_;

  std::vector<std::thread> dispatcherThreads_;
  std::atomic<bool> stopFlag_{false};
};

}  // namespace monix::eventbus
