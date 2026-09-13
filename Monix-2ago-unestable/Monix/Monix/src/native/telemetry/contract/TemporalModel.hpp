#pragma once

#include <cstdint>

#include "SensorState.hpp"

namespace monix::telemetry {

enum class ObservationKind {
  State,
  Transition,
  Incident,
  Recovery
};

inline const char* ObservationKindName(ObservationKind k) {
  switch (k) {
    case ObservationKind::State:      return "State";
    case ObservationKind::Transition: return "Transition";
    case ObservationKind::Incident:   return "Incident";
    case ObservationKind::Recovery:   return "Recovery";
  }
  return "Unknown";
}

struct TemporalEvent {
  ObservationKind kind = ObservationKind::State;
  const char* entity = nullptr;
  const char* attribute = nullptr;
  SensorState previous = SensorState::Initializing;
  SensorState current = SensorState::Initializing;
  std::uint64_t startedAtNs = 0;
  std::uint64_t detectedAtNs = 0;
  int consecutiveCount = 0;
};

}
