#pragma once

#include <cstdint>

namespace monix::collectors {

enum class CollectorCapability : std::uint32_t {
  None       = 0,
  Realtime   = 1 << 0,
  Snapshot   = 1 << 1,
  Polling    = 1 << 2,
  Recovery   = 1 << 3,
  External   = 1 << 4,
  Correlation = 1 << 5
};

inline CollectorCapability operator|(CollectorCapability a, CollectorCapability b) {
  return static_cast<CollectorCapability>(
    static_cast<std::uint32_t>(a) | static_cast<std::uint32_t>(b));
}

inline CollectorCapability operator&(CollectorCapability a, CollectorCapability b) {
  return static_cast<CollectorCapability>(
    static_cast<std::uint32_t>(a) & static_cast<std::uint32_t>(b));
}

inline CollectorCapability& operator|=(CollectorCapability& a, CollectorCapability b) {
  a = a | b;
  return a;
}

inline bool hasCapability(CollectorCapability caps, CollectorCapability flag) {
  return (static_cast<std::uint32_t>(caps) & static_cast<std::uint32_t>(flag)) != 0;
}

}  // namespace monix::collectors
