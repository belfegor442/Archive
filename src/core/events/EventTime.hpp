#pragma once

#include <cstdint>
#include <string>
#include <chrono>

namespace monix::events {

using Timestamp = std::int64_t;
using MonotonicNanos = std::uint64_t;

struct EventTime {
  Timestamp occurrence = 0;
  Timestamp ingestion = 0;

  bool isValid() const { return occurrence > 0 && ingestion > 0; }
  double latencyMs() const;
};

struct MonotonicTime {
  MonotonicNanos nanoseconds = 0;

  static MonotonicTime now();
};

Timestamp currentSystemTimeMs();
Timestamp currentSystemTimeNs();

}  // namespace monix::events
