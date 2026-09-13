#pragma once

#include <cstdint>
#include <string>

namespace monix::collectors::driver {

using DriverId = std::uint64_t;

enum class DriverEventKind : std::uint8_t {
  Loaded,
  Unloaded,
  Changed
};

const char* DriverEventKindName(DriverEventKind k);
std::string DriverEventKindAction(DriverEventKind k);

enum class DriverState : std::uint8_t {
  Running,
  Stopped,
  StartPending,
  StopPending,
  Unknown
};

const char* DriverStateName(DriverState s);

enum class DriverDetectionOrigin : std::uint8_t {
  Registry,
  EnumDeviceDrivers,
  Manual,
  Polling
};

const char* DriverDetectionOriginName(DriverDetectionOrigin o);

struct DriverInfo {
  DriverId id = 0;
  std::string name;
  std::string path;
  std::string version;
  std::string provider;
  DriverState state = DriverState::Unknown;
  std::uint64_t base_address = 0;
  std::int64_t timestamp_ms = 0;

  bool isValid() const;
  bool isRunning() const;
  std::string summary() const;
};

}  // namespace monix::collectors::driver
