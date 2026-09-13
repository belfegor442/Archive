#pragma once

#include <cstdint>
#include <type_traits>

#include "SensorState.hpp"

namespace monix::telemetry {

template<typename T>
struct Measurement {
  T value{};
  SensorState state = SensorState::Initializing;
  std::uint64_t timestampNs = 0;
  std::uint64_t sequence = 0;
  float confidence = 0.0f;
  bool isEstimate = false;
  const char* source = nullptr;
  const char* sensor = nullptr;
  const char* error = nullptr;

  bool IsAvailable() const { return state == SensorState::Available; }
  bool HasValue() const { return state == SensorState::Available || state == SensorState::Stale; }

  static Measurement Unavailable(const char* src = nullptr, const char* sens = nullptr) {
    Measurement m;
    m.state = SensorState::Unavailable;
    m.source = src;
    m.sensor = sens;
    return m;
  }

  static Measurement NotSupported(const char* src = nullptr, const char* sens = nullptr) {
    Measurement m;
    m.state = SensorState::NotSupported;
    m.source = src;
    m.sensor = sens;
    return m;
  }

  static Measurement Error(const char* errMsg, const char* src = nullptr, const char* sens = nullptr) {
    Measurement m;
    m.state = SensorState::Error;
    m.error = errMsg;
    m.source = src;
    m.sensor = sens;
    return m;
  }

  static Measurement Stale(T val, std::uint64_t tsNs, const char* src = nullptr, const char* sens = nullptr) {
    Measurement m;
    m.value = val;
    m.state = SensorState::Stale;
    m.timestampNs = tsNs;
    m.source = src;
    m.sensor = sens;
    return m;
  }
};

}
