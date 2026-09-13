#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdint>
#include <string>

#include "../core/Types.hpp"
#include "../settings/MonixConfigTypes.hpp"
#include "../telemetry/TelemetryEvents.hpp"

namespace monix {

struct LogEntry {
  std::wstring time;
  std::wstring fullTimestamp;
  std::wstring firstTime;
  std::wstring firstFullTimestamp;
  std::wstring domain;
  std::wstring severity;
  std::wstring module;
  std::wstring service;
  std::wstring message;
  std::wstring metadata;
  std::wstring sessionId;
  std::wstring userId;
  std::uint64_t eventId = 0;
  std::uint64_t sequenceId = 0;
  std::uint64_t createdMonotonicNs = 0;
  DWORD processId = 0;
  DWORD threadId = 0;
  LogLevel level = LogLevel::Info;
  ColorRole color = ColorRole::White;
  EventType eventType = EventType::TelemetrySample;
  int repeatCount = 1;
  std::wstring correlationId;
};

struct SessionCounters {
  int debug = 0;
  int info = 0;
  int warnings = 0;
  int errors = 0;
  int critical = 0;
  int network = 0;
  int ai = 0;
  int kernel = 0;
  int userInput = 0;
  int storage = 0;
};

}
