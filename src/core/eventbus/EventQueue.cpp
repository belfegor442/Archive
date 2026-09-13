#include "EventQueue.hpp"

namespace monix::eventbus {

EventQueue::EventQueue(const EventBusConfig& config)
  : config_(config), backpressure_(config.queue_capacity) {}

bool EventQueue::push(BusEvent event) {
  std::unique_lock<std::mutex> lock(mu_);
  if (closed_) return false;
  if (queue_.size() >= config_.queue_capacity) {
    droppedCount_++;
    return false;
  }
  queue_.push_back(std::move(event));
  backpressure_.update(queue_.size());
  notEmpty_.notify_one();
  return true;
}

bool EventQueue::pushWithPriority(BusEvent event, EventPriority priority) {
  std::unique_lock<std::mutex> lock(mu_);
  if (closed_) return false;
  if (queue_.size() >= config_.queue_capacity) {
    switch (config_.drop_policy) {
      case DropPolicy::BlockProducer:
        notFull_.wait(lock, [this] {
          return closed_ || queue_.size() < config_.queue_capacity;
        });
        if (closed_) return false;
        break;
      case DropPolicy::DropOldest:
        queue_.pop_front();
        droppedCount_++;
        break;
      case DropPolicy::DropByPriority: {
        auto lowest = std::min_element(queue_.begin(), queue_.end(),
          [](const BusEvent& left, const BusEvent& right) {
            return left.priority < right.priority;
          });
        if (lowest == queue_.end() || lowest->priority >= priority) {
          droppedCount_++;
          return false;
        }
        queue_.erase(lowest);
        droppedCount_++;
        break;
      }
      case DropPolicy::Adaptive:
        if (priority < EventPriority::High) {
          droppedCount_++;
          return false;
        }
        queue_.pop_front();
        droppedCount_++;
        break;
      case DropPolicy::RejectNewest:
        droppedCount_++;
        return false;
    }
  }
  queue_.push_back(std::move(event));
  backpressure_.update(queue_.size());
  notEmpty_.notify_one();
  return true;
}

std::optional<BusEvent> EventQueue::pop() {
  std::unique_lock<std::mutex> lock(mu_);
  if (queue_.empty()) return std::nullopt;
  auto event = std::move(queue_.front());
  queue_.pop_front();
  backpressure_.update(queue_.size());
  notFull_.notify_one();
  return event;
}

bool EventQueue::pop(BusEvent& out) {
  std::unique_lock<std::mutex> lock(mu_);
  if (queue_.empty()) return false;
  out = std::move(queue_.front());
  queue_.pop_front();
  backpressure_.update(queue_.size());
  notFull_.notify_one();
  return true;
}

std::size_t EventQueue::size() const {
  std::lock_guard<std::mutex> lock(mu_);
  return queue_.size();
}

bool EventQueue::empty() const {
  std::lock_guard<std::mutex> lock(mu_);
  return queue_.empty();
}

bool EventQueue::full() const {
  std::lock_guard<std::mutex> lock(mu_);
  return queue_.size() >= config_.queue_capacity;
}

void EventQueue::clear() {
  std::lock_guard<std::mutex> lock(mu_);
  queue_.clear();
  backpressure_.update(0);
}

void EventQueue::close() {
  std::lock_guard<std::mutex> lock(mu_);
  closed_ = true;
  notEmpty_.notify_all();
}

void EventQueue::reopen() {
  std::lock_guard<std::mutex> lock(mu_);
  closed_ = false;
  backpressure_.update(queue_.size());
}

QueuePressure EventQueue::pressure() const {
  return backpressure_.pressure();
}

}  // namespace monix::eventbus
