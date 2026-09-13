#pragma once

#include "EventFilter.hpp"

#include <cstdint>
#include <atomic>

namespace monix::eventbus {

enum class QueuePressure : std::uint8_t {
  Normal,
  Elevated,
  High,
  Critical,
  Full
};

const char* QueuePressureName(QueuePressure p);

class BackpressureController {
public:
  explicit BackpressureController(std::size_t capacity);

  void update(std::size_t currentDepth);
  QueuePressure pressure() const;

  std::size_t capacity() const { return capacity_; }
  std::size_t currentDepth() const { return currentDepth_; }
  double fillRatio() const;

  bool shouldDrop(EventPriority priority) const;
  bool shouldReject() const;

private:
  std::size_t capacity_;
  std::atomic<std::size_t> currentDepth_{0};
  std::atomic<QueuePressure> pressure_{QueuePressure::Normal};
};

}  // namespace monix::eventbus
