#pragma once

#include "../events/EventId.hpp"
#include "../events/EventTime.hpp"

#include <cstdint>
#include <string>
#include <vector>
#include <mutex>

namespace monix::eventbus {

using ConsumerId = std::string;

struct DeadLetterEntry {
  events::EventId event_id;
  ConsumerId consumer_id;
  std::string failure_reason;
  std::uint32_t retry_count = 0;
  events::Timestamp first_failure = 0;
  events::Timestamp last_failure = 0;
};

class DeadLetterQueue {
public:
  void push(DeadLetterEntry entry);
  std::vector<DeadLetterEntry> drain();
  std::size_t size() const;
  void clear();

private:
  mutable std::mutex mu_;
  std::vector<DeadLetterEntry> entries_;
};

}  // namespace monix::eventbus
