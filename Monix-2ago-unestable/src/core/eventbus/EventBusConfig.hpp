#pragma once

#include <cstdint>
#include <chrono>

namespace monix::eventbus {

enum class DropPolicy : std::uint8_t {
  RejectNewest,
  DropOldest,
  DropByPriority,
  BlockProducer,
  Adaptive
};

struct EventBusConfig {
  std::size_t queue_capacity = 100000;
  std::size_t consumer_worker_count = 4;
  DropPolicy drop_policy = DropPolicy::DropByPriority;
  std::size_t max_retries = 3;
  std::chrono::milliseconds retry_delay{100};
  std::chrono::milliseconds shutdown_timeout{5000};
  bool enable_priority = true;
  bool enable_metrics = true;
};

}  // namespace monix::eventbus
