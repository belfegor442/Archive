#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <cstdint>
#include <string>

namespace monix {

enum class EventCategory : uint8_t {
  System, Process, Network, Hardware, Thermal,
  Power, Storage, Security, Configuration,
  User, Scram, Diagnostic, Filesystem, Registry,
  Audio, Display, Driver, Renderer
};

enum class EventSeverity : uint8_t {
  Info = 0, Low = 1, Medium = 2, High = 3, Critical = 4
};

inline const wchar_t* EventSeverityName(EventSeverity s) {
  switch (s) {
    case EventSeverity::Info:     return L"INFO";
    case EventSeverity::Low:      return L"LOW";
    case EventSeverity::Medium:   return L"MEDIUM";
    case EventSeverity::High:     return L"HIGH";
    case EventSeverity::Critical: return L"CRITICAL";
  }
  return L"UNKNOWN";
}

inline const wchar_t* EventCategoryName(EventCategory c) {
  switch (c) {
    case EventCategory::System:        return L"SYSTEM";
    case EventCategory::Process:       return L"PROCESS";
    case EventCategory::Network:       return L"NETWORK";
    case EventCategory::Hardware:      return L"HARDWARE";
    case EventCategory::Thermal:       return L"THERMAL";
    case EventCategory::Power:         return L"POWER";
    case EventCategory::Storage:       return L"STORAGE";
    case EventCategory::Security:      return L"SECURITY";
    case EventCategory::Configuration: return L"CONFIGURATION";
    case EventCategory::User:          return L"USER";
    case EventCategory::Scram:         return L"SCRAM";
    case EventCategory::Diagnostic:    return L"DIAGNOSTIC";
    case EventCategory::Filesystem:    return L"FILESYSTEM";
    case EventCategory::Registry:      return L"REGISTRY";
    case EventCategory::Audio:         return L"AUDIO";
    case EventCategory::Display:       return L"DISPLAY";
    case EventCategory::Driver:        return L"DRIVER";
    case EventCategory::Renderer:      return L"RENDERER";
  }
  return L"UNKNOWN";
}

struct SystemEvent {
  uint64_t id = 0;
  uint64_t timestampNs = 0;
  EventCategory category = EventCategory::System;
  EventSeverity severity = EventSeverity::Info;
  std::wstring type;
  std::wstring description;
  std::wstring subsystem;
  int processId = 0;
  std::wstring processName;
  uint64_t snapshotIdBefore = 0;
  uint64_t snapshotIdAfter = 0;
  DWORD threadId = 0;
  std::wstring correlationId;
  std::wstring metadata;
  bool uiActionable = false;
  bool structuredOnly = false;
};

}
