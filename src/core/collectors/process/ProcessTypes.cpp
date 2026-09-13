#include "ProcessTypes.hpp"

namespace monix::collectors::proc {

const char* ProcessArchitectureName(ProcessArchitecture a) {
  switch (a) {
    case ProcessArchitecture::Unknown: return "Unknown";
    case ProcessArchitecture::X86:     return "x86";
    case ProcessArchitecture::X64:     return "x64";
    case ProcessArchitecture::ARM:     return "ARM";
    case ProcessArchitecture::ARM64:   return "ARM64";
  }
  return "Unknown";
}

const char* ProcessIntegrityLevelName(ProcessIntegrityLevel l) {
  switch (l) {
    case ProcessIntegrityLevel::Unknown:  return "Unknown";
    case ProcessIntegrityLevel::Untrusted: return "Untrusted";
    case ProcessIntegrityLevel::Low:      return "Low";
    case ProcessIntegrityLevel::Medium:   return "Medium";
    case ProcessIntegrityLevel::High:     return "High";
    case ProcessIntegrityLevel::System:   return "System";
  }
  return "Unknown";
}

const char* ProcessEventKindName(ProcessEventKind k) {
  switch (k) {
    case ProcessEventKind::Started:    return "Started";
    case ProcessEventKind::Terminated: return "Terminated";
    case ProcessEventKind::Suspended:  return "Suspended";
    case ProcessEventKind::Resumed:    return "Resumed";
  }
  return "Unknown";
}

std::string ProcessEventKindAction(ProcessEventKind k) {
  switch (k) {
    case ProcessEventKind::Started:    return "started";
    case ProcessEventKind::Terminated: return "terminated";
    case ProcessEventKind::Suspended:  return "suspended";
    case ProcessEventKind::Resumed:    return "resumed";
  }
  return "unknown";
}

const char* ObservationOriginName(ObservationOrigin o) {
  switch (o) {
    case ObservationOrigin::InitialSnapshot: return "InitialSnapshot";
    case ObservationOrigin::Polling:         return "Polling";
    case ObservationOrigin::Manual:          return "Manual";
  }
  return "Unknown";
}

std::int64_t ProcessInfo::cpu_time_ms() const {
  return static_cast<std::int64_t>(
    (kernel_time_100ns + user_time_100ns) / 10000);
}

bool ProcessInfo::is_alive() const {
  return exit_time_ms == 0;
}

bool ProcessInfo::is_64bit() const {
  return architecture == ProcessArchitecture::X64 ||
         architecture == ProcessArchitecture::ARM64;
}

}  // namespace monix::collectors::proc
