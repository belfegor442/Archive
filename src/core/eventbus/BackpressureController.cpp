#include "BackpressureController.hpp"

namespace monix::eventbus {

const char* QueuePressureName(QueuePressure p) {
  switch (p) {
    case QueuePressure::Normal:   return "Normal";
    case QueuePressure::Elevated: return "Elevated";
    case QueuePressure::High:     return "High";
    case QueuePressure::Critical: return "Critical";
    case QueuePressure::Full:     return "Full";
  }
  return "Unknown";
}

BackpressureController::BackpressureController(std::size_t capacity)
  : capacity_(capacity) {}

void BackpressureController::update(std::size_t currentDepth) {
  currentDepth_.store(currentDepth, std::memory_order_relaxed);
  double ratio = capacity_ > 0 ? static_cast<double>(currentDepth) / static_cast<double>(capacity_) : 0.0;

  QueuePressure p;
  if (ratio >= 1.0)       p = QueuePressure::Full;
  else if (ratio >= 0.9)  p = QueuePressure::Critical;
  else if (ratio >= 0.7)  p = QueuePressure::High;
  else if (ratio >= 0.5)  p = QueuePressure::Elevated;
  else                    p = QueuePressure::Normal;

  pressure_.store(p, std::memory_order_relaxed);
}

QueuePressure BackpressureController::pressure() const {
  return pressure_.load(std::memory_order_relaxed);
}

double BackpressureController::fillRatio() const {
  std::size_t cap = capacity_;
  std::size_t depth = currentDepth_.load(std::memory_order_relaxed);
  return cap > 0 ? static_cast<double>(depth) / static_cast<double>(cap) : 0.0;
}

bool BackpressureController::shouldDrop(EventPriority priority) const {
  QueuePressure p = pressure();
  switch (p) {
    case QueuePressure::Normal:
    case QueuePressure::Elevated:
      return false;
    case QueuePressure::High:
      return priority <= EventPriority::Background;
    case QueuePressure::Critical:
      return priority <= EventPriority::Normal;
    case QueuePressure::Full:
      return priority <= EventPriority::High;
  }
  return false;
}

bool BackpressureController::shouldReject() const {
  return pressure() == QueuePressure::Full;
}

}  // namespace monix::eventbus
