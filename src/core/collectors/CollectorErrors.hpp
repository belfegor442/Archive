#pragma once

#include <cstdint>
#include <string>

namespace monix::collectors {

enum class CollectorErrorCode : std::uint16_t {
  None = 0,
  AlreadyRegistered,
  NotRegistered,
  AlreadyRunning,
  NotRunning,
  StartFailed,
  StopFailed,
  InvalidTransition,
  InvalidConfig,
  ResourceBusy,
  DependencyMissing,
  InternalError
};

const char* CollectorErrorCodeName(CollectorErrorCode c);

}  // namespace monix::collectors
