#pragma once

#include <cstdint>

namespace monix::events {

enum class EventFlags : std::uint32_t {
  None       = 0,
  Synthetic  = 1 << 0,
  Derived    = 1 << 1,
  Aggregated = 1 << 2,
  Sampled    = 1 << 3,
  Late       = 1 << 4,
  Partial    = 1 << 5
};

inline EventFlags operator|(EventFlags a, EventFlags b) noexcept {
  return static_cast<EventFlags>(static_cast<std::uint32_t>(a) | static_cast<std::uint32_t>(b));
}

inline EventFlags operator&(EventFlags a, EventFlags b) noexcept {
  return static_cast<EventFlags>(static_cast<std::uint32_t>(a) & static_cast<std::uint32_t>(b));
}

inline EventFlags& operator|=(EventFlags& a, EventFlags b) noexcept {
  a = a | b;
  return a;
}

inline bool hasFlag(EventFlags flags, EventFlags flag) noexcept {
  return (static_cast<std::uint32_t>(flags) & static_cast<std::uint32_t>(flag)) != 0;
}

}  // namespace monix::events
