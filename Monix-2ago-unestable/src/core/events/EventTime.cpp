#include "EventTime.hpp"

namespace monix::events {

double EventTime::latencyMs() const {
  if (!isValid()) return 0.0;
  return static_cast<double>(ingestion - occurrence);
}

MonotonicTime MonotonicTime::now() {
  auto tp = std::chrono::steady_clock::now().time_since_epoch();
  return MonotonicTime{
    static_cast<MonotonicNanos>(std::chrono::duration_cast<std::chrono::nanoseconds>(tp).count())
  };
}

Timestamp currentSystemTimeMs() {
  return static_cast<Timestamp>(
    std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count());
}

Timestamp currentSystemTimeNs() {
  return static_cast<Timestamp>(
    std::chrono::duration_cast<std::chrono::nanoseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count());
}

}  // namespace monix::events
