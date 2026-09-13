#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "../Snapshot.hpp"

namespace monix::telemetry {

class TimelineRing {
public:
  static constexpr std::size_t kDefaultCapacity = 64;

  explicit TimelineRing(std::size_t cap = kDefaultCapacity)
    : ring_(cap), capacity_(cap) {}

  void Push(const Snapshot& snap) {
    ring_[head_] = snap;
    head_ = (head_ + 1) % capacity_;
    if (count_ < capacity_) count_++;
  }

  const Snapshot& Current() const {
    return ring_[(head_ + capacity_ - 1) % capacity_];
  }

  const Snapshot* Previous() const {
    return count_ < 2 ? nullptr : &ring_[(head_ + capacity_ - 2) % capacity_];
  }

  const Snapshot* At(std::size_t offset) const {
    if (offset >= count_) return nullptr;
    return &ring_[(head_ + capacity_ - 1 - offset) % capacity_];
  }

  std::size_t Count() const { return count_; }
  std::size_t Capacity() const { return capacity_; }
  bool Full() const { return count_ == capacity_; }

  void Clear() {
    head_ = 0;
    count_ = 0;
  }

private:
  std::vector<Snapshot> ring_;
  std::size_t capacity_ = 0;
  std::size_t head_ = 0;
  std::size_t count_ = 0;
};

}
