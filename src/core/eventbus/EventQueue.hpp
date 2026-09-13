#pragma once

#include "EventBusConfig.hpp"
#include "EventBusErrors.hpp"
#include "EventFilter.hpp"
#include "EventProducer.hpp"
#include "BackpressureController.hpp"

#include "../events/Event.hpp"
#include "../events/EventTime.hpp"

#include <cstdint>
#include <deque>
#include <mutex>
#include <condition_variable>
#include <optional>

namespace monix::eventbus {

class EventQueue {
public:
  explicit EventQueue(const EventBusConfig& config);

  bool push(BusEvent event);
  std::optional<BusEvent> pop();
  bool pop(BusEvent& out);

  bool pushWithPriority(BusEvent event, EventPriority priority);

  std::size_t size() const;
  bool empty() const;
  bool full() const;
  void clear();
  void close();
  void reopen();

  QueuePressure pressure() const;
  std::size_t capacity() const { return config_.queue_capacity; }

  const BackpressureController& backpressure() const { return backpressure_; }

private:
  EventBusConfig config_;
  mutable std::mutex mu_;
  std::condition_variable notEmpty_;
  std::condition_variable notFull_;

  std::deque<BusEvent> queue_;
  std::size_t droppedCount_ = 0;
  bool closed_ = false;

  BackpressureController backpressure_;
};

}  // namespace monix::eventbus
