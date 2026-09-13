#pragma once

#include "ValidationTypes.hpp"
#include "EventSchema.hpp"
#include "../events/EventTime.hpp"

#include <cstdint>
#include <chrono>

namespace monix::validation {

struct ValidationPolicy {
  bool reject_unknown_schema = false;
  DropPolicy unknown_fields = DropPolicy::Warn;
  std::chrono::seconds max_future_skew{30};
  std::chrono::seconds max_past_skew{86400};
  bool verify_integrity = false;
  bool verify_sequence = false;
  ValidationProfile profile = ValidationProfile::Standard;

  static ValidationPolicy fast();
  static ValidationPolicy standard();
  static ValidationPolicy strict();
  static ValidationPolicy forensic();
};

}  // namespace monix::validation
