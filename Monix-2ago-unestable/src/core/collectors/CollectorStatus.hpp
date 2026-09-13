#pragma once

#include <cstdint>
#include <string>

namespace monix::collectors {

enum class CollectorLifecycle : std::uint8_t {
  Created,
  Initializing,
  Running,
  Degraded,
  Stopping,
  Stopped,
  Failed
};

const char* CollectorLifecycleName(CollectorLifecycle s);

struct CollectorStatus {
  CollectorLifecycle lifecycle = CollectorLifecycle::Created;
  std::string message;
  std::uint64_t last_event_count = 0;
  double error_rate = 0.0;

  bool canStart() const;
  bool canStop() const;
  bool isOperational() const;
};

bool isValidTransition(CollectorLifecycle from, CollectorLifecycle to);

}  // namespace monix::collectors
