#pragma once

#include <cstdint>
#include <string>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace monix::collectors::service {

using ServiceId = std::uint64_t;

enum class ServiceEventKind : std::uint8_t {
  Created,
  Deleted,
  Started,
  Stopped,
  Changed
};

const char* ServiceEventKindName(ServiceEventKind k);
std::string ServiceEventKindAction(ServiceEventKind k);

enum class ServiceState : std::uint8_t {
  Stopped,
  StartPending,
  StopPending,
  Running,
  ContinuePending,
  PausePending,
  Paused,
  Unknown
};

const char* ServiceStateName(ServiceState s);
ServiceState ServiceStateFromDword(DWORD state);

enum class ServiceStartType : std::uint8_t {
  Boot,
  System,
  AutoStart,
  Demand,
  Disabled,
  Unknown
};

const char* ServiceStartTypeName(ServiceStartType t);
ServiceStartType ServiceStartTypeFromDword(DWORD type);

enum class ServiceDetectionOrigin : std::uint8_t {
  SCM,
  Manual,
  Polling
};

const char* ServiceDetectionOriginName(ServiceDetectionOrigin o);

struct ServiceInfo {
  ServiceId id = 0;
  std::string name;
  std::string display_name;
  ServiceState state = ServiceState::Unknown;
  ServiceStartType start_type = ServiceStartType::Unknown;
  std::string account;
  std::string binary_path;
  std::string description;
  std::uint32_t process_id = 0;
  std::int64_t timestamp_ms = 0;

  bool isValid() const;
  bool isRunning() const;
  bool isStopped() const;
  std::string summary() const;
};

}  // namespace monix::collectors::service
