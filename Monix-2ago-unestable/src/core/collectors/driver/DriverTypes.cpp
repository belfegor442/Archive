#include "DriverTypes.hpp"

namespace monix::collectors::driver {

const char* DriverEventKindName(DriverEventKind k) {
  switch (k) {
    case DriverEventKind::Loaded:   return "Loaded";
    case DriverEventKind::Unloaded: return "Unloaded";
    case DriverEventKind::Changed:  return "Changed";
  }
  return "Unknown";
}

std::string DriverEventKindAction(DriverEventKind k) {
  switch (k) {
    case DriverEventKind::Loaded:   return "driver.loaded";
    case DriverEventKind::Unloaded: return "driver.unloaded";
    case DriverEventKind::Changed:  return "driver.changed";
  }
  return "unknown";
}

const char* DriverStateName(DriverState s) {
  switch (s) {
    case DriverState::Running:      return "Running";
    case DriverState::Stopped:      return "Stopped";
    case DriverState::StartPending: return "StartPending";
    case DriverState::StopPending:  return "StopPending";
    case DriverState::Unknown:      return "Unknown";
  }
  return "Unknown";
}

const char* DriverDetectionOriginName(DriverDetectionOrigin o) {
  switch (o) {
    case DriverDetectionOrigin::Registry:         return "Registry";
    case DriverDetectionOrigin::EnumDeviceDrivers: return "EnumDeviceDrivers";
    case DriverDetectionOrigin::Manual:           return "Manual";
    case DriverDetectionOrigin::Polling:          return "Polling";
  }
  return "Unknown";
}

bool DriverInfo::isValid() const {
  return !name.empty();
}

bool DriverInfo::isRunning() const {
  return state == DriverState::Running;
}

std::string DriverInfo::summary() const {
  std::string result = name;
  if (!version.empty()) result += " v" + version;
  if (!provider.empty()) result += " by " + provider;
  return result;
}

}  // namespace monix::collectors::driver
