#include "EventId.hpp"

#include <algorithm>
#include <random>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <cstring>

namespace monix::events {

namespace {

thread_local std::mt19937_64 rng{std::random_device{}()};
thread_local std::uniform_int_distribution<std::uint32_t> dist{0, 0xFFFFFFFF};

std::uint64_t currentTimestampMs() {
  return static_cast<std::uint64_t>(
    std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count());
}

int hexCharToNibble(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

}  // namespace

EventId::EventId() : value_{} {}

EventId EventId::generate() {
  std::array<std::uint8_t, 16> bytes{};

  std::uint64_t ts = currentTimestampMs();
  bytes[0] = static_cast<std::uint8_t>((ts >> 40) & 0xFF);
  bytes[1] = static_cast<std::uint8_t>((ts >> 32) & 0xFF);
  bytes[2] = static_cast<std::uint8_t>((ts >> 24) & 0xFF);
  bytes[3] = static_cast<std::uint8_t>((ts >> 16) & 0xFF);
  bytes[4] = static_cast<std::uint8_t>((ts >> 8) & 0xFF);
  bytes[5] = static_cast<std::uint8_t>(ts & 0xFF);

  std::uint32_t rand1 = dist(rng);
  std::uint32_t rand2 = dist(rng);

  bytes[6] = static_cast<std::uint8_t>(0x70 | ((rand1 >> 28) & 0x0F));
  bytes[7] = static_cast<std::uint8_t>((rand1 >> 20) & 0xFF);
  bytes[8] = static_cast<std::uint8_t>(0x80 | ((rand1 >> 12) & 0x3F));
  bytes[9] = static_cast<std::uint8_t>((rand1 >> 4) & 0xFF);
  bytes[10] = static_cast<std::uint8_t>(((rand1 & 0x0F) << 4) | ((rand2 >> 28) & 0x0F));
  bytes[11] = static_cast<std::uint8_t>((rand2 >> 20) & 0xFF);
  bytes[12] = static_cast<std::uint8_t>((rand2 >> 12) & 0xFF);
  bytes[13] = static_cast<std::uint8_t>((rand2 >> 4) & 0xFF);
  bytes[14] = static_cast<std::uint8_t>(((rand2 & 0x0F) << 4) | (dist(rng) & 0x0F));
  bytes[15] = static_cast<std::uint8_t>(dist(rng) & 0xFF);

  return EventId(bytes);
}

EventId EventId::fromString(const std::string& str) {
  std::string cleaned = str;
  cleaned.erase(std::remove(cleaned.begin(), cleaned.end(), '-'), cleaned.end());

  if (cleaned.size() != 32) return EventId{};

  std::array<std::uint8_t, 16> bytes{};
  for (size_t i = 0; i < 16; ++i) {
    int hi = hexCharToNibble(cleaned[i * 2]);
    int lo = hexCharToNibble(cleaned[i * 2 + 1]);
    if (hi < 0 || lo < 0) return EventId{};
    bytes[i] = static_cast<std::uint8_t>((hi << 4) | lo);
  }

  return EventId(bytes);
}

EventId EventId::fromBytes(const std::array<std::uint8_t, 16>& bytes) {
  return EventId(bytes);
}

std::string EventId::toString() const {
  std::ostringstream ss;
  ss << std::hex << std::setfill('0');
  for (size_t i = 0; i < 16; ++i) {
    ss << std::setw(2) << static_cast<int>(value_[i]);
    if (i == 3 || i == 5 || i == 7 || i == 9) ss << '-';
  }
  return ss.str();
}

std::array<std::uint8_t, 16> EventId::toBytes() const {
  return value_;
}

bool EventId::isValid() const {
  return value_ != std::array<std::uint8_t, 16>{};
}

bool EventId::operator==(const EventId& other) const noexcept {
  return value_ == other.value_;
}

bool EventId::operator!=(const EventId& other) const noexcept {
  return value_ != other.value_;
}

bool EventId::operator<(const EventId& other) const noexcept {
  return value_ < other.value_;
}

EventId::EventId(std::array<std::uint8_t, 16> bytes) : value_(bytes) {}

}  // namespace monix::events
