#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <optional>

namespace monix::events {

class EventId {
public:
  EventId();

  static EventId generate();
  static EventId fromString(const std::string& str);
  static EventId fromBytes(const std::array<std::uint8_t, 16>& bytes);

  std::string toString() const;
  std::array<std::uint8_t, 16> toBytes() const;

  bool isValid() const;

  bool operator==(const EventId& other) const noexcept;
  bool operator!=(const EventId& other) const noexcept;
  bool operator<(const EventId& other) const noexcept;

private:
  explicit EventId(std::array<std::uint8_t, 16> bytes);

  std::array<std::uint8_t, 16> value_;
};

}  // namespace monix::events
