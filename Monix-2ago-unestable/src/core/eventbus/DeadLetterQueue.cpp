#include "DeadLetterQueue.hpp"

namespace monix::eventbus {

void DeadLetterQueue::push(DeadLetterEntry entry) {
  std::lock_guard<std::mutex> lock(mu_);
  entries_.push_back(std::move(entry));
}

std::vector<DeadLetterEntry> DeadLetterQueue::drain() {
  std::lock_guard<std::mutex> lock(mu_);
  auto result = std::move(entries_);
  entries_.clear();
  return result;
}

std::size_t DeadLetterQueue::size() const {
  std::lock_guard<std::mutex> lock(mu_);
  return entries_.size();
}

void DeadLetterQueue::clear() {
  std::lock_guard<std::mutex> lock(mu_);
  entries_.clear();
}

}  // namespace monix::eventbus
