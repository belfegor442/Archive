#pragma once

namespace monix::telemetry {

enum class SensorState {
  Initializing,
  Available,
  Unavailable,
  Stale,
  Error,
  NotSupported
};

inline const char* SensorStateName(SensorState s) {
  switch (s) {
    case SensorState::Initializing:  return "Initializing";
    case SensorState::Available:     return "Available";
    case SensorState::Unavailable:   return "Unavailable";
    case SensorState::Stale:         return "Stale";
    case SensorState::Error:         return "Error";
    case SensorState::NotSupported:  return "NotSupported";
  }
  return "Unknown";
}

}
