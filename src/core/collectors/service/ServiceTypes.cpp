#include "ServiceTypes.hpp"

#include <algorithm>
#include <cctype>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace monix::collectors::service {

const char* ServiceEventKindName(ServiceEventKind k) {
  switch (k) {
    case ServiceEventKind::Created:  return "Created";
    case ServiceEventKind::Deleted:  return "Deleted";
    case ServiceEventKind::Started:  return "Started";
    case ServiceEventKind::Stopped:  return "Stopped";
    case ServiceEventKind::Changed:  return "Changed";
  }
  return "Unknown";
}

std::string ServiceEventKindAction(ServiceEventKind k) {
  switch (k) {
    case ServiceEventKind::Created:  return "service.created";
    case ServiceEventKind::Deleted:  return "service.deleted";
    case ServiceEventKind::Started:  return "service.started";
    case ServiceEventKind::Stopped:  return "service.stopped";
    case ServiceEventKind::Changed:  return "service.changed";
  }
  return "unknown";
}

const char* ServiceStateName(ServiceState s) {
  switch (s) {
    case ServiceState::Stopped:       return "Stopped";
    case ServiceState::StartPending:  return "StartPending";
    case ServiceState::StopPending:   return "StopPending";
    case ServiceState::Running:       return "Running";
    case ServiceState::ContinuePending: return "ContinuePending";
    case ServiceState::PausePending:  return "PausePending";
    case ServiceState::Paused:        return "Paused";
    case ServiceState::Unknown:       return "Unknown";
  }
  return "Unknown";
}

ServiceState ServiceStateFromDword(DWORD state) {
  switch (state) {
    case SERVICE_STOPPED:          return ServiceState::Stopped;
    case SERVICE_START_PENDING:    return ServiceState::StartPending;
    case SERVICE_STOP_PENDING:     return ServiceState::StopPending;
    case SERVICE_RUNNING:          return ServiceState::Running;
    case SERVICE_CONTINUE_PENDING: return ServiceState::ContinuePending;
    case SERVICE_PAUSE_PENDING:    return ServiceState::PausePending;
    case SERVICE_PAUSED:           return ServiceState::Paused;
    default:                       return ServiceState::Unknown;
  }
}

const char* ServiceStartTypeName(ServiceStartType t) {
  switch (t) {
    case ServiceStartType::Boot:       return "Boot";
    case ServiceStartType::System:     return "System";
    case ServiceStartType::AutoStart:  return "AutoStart";
    case ServiceStartType::Demand:     return "Demand";
    case ServiceStartType::Disabled:   return "Disabled";
    case ServiceStartType::Unknown:    return "Unknown";
  }
  return "Unknown";
}

ServiceStartType ServiceStartTypeFromDword(DWORD type) {
  switch (type) {
    case SERVICE_BOOT_START:   return ServiceStartType::Boot;
    case SERVICE_SYSTEM_START: return ServiceStartType::System;
    case SERVICE_AUTO_START:   return ServiceStartType::AutoStart;
    case SERVICE_DEMAND_START: return ServiceStartType::Demand;
    case SERVICE_DISABLED:     return ServiceStartType::Disabled;
    default:                   return ServiceStartType::Unknown;
  }
}

const char* ServiceDetectionOriginName(ServiceDetectionOrigin o) {
  switch (o) {
    case ServiceDetectionOrigin::SCM:      return "SCM";
    case ServiceDetectionOrigin::Manual:   return "Manual";
    case ServiceDetectionOrigin::Polling:  return "Polling";
  }
  return "Unknown";
}

bool ServiceInfo::isValid() const {
  return !name.empty();
}

bool ServiceInfo::isRunning() const {
  return state == ServiceState::Running;
}

bool ServiceInfo::isStopped() const {
  return state == ServiceState::Stopped;
}

std::string ServiceInfo::summary() const {
  if (!display_name.empty()) return display_name;
  if (!name.empty()) return name;
  return "Unknown Service";
}

}  // namespace monix::collectors::service
