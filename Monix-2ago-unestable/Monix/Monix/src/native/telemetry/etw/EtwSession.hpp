#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <evntrace.h>

#include <cstdint>
#include <string>
#include <vector>

namespace monix::etw {

enum class SessionState {
  Stopped,
  Running,
  Error
};

struct SessionConfig {
  std::wstring name = L"MonixSession";
  std::uint64_t bufferSizeKB = 1024;
  std::uint64_t minBuffers = 2;
  std::uint64_t maxBuffers = 16;
  std::uint64_t flushTimerSec = 1;
  bool realTime = true;
};

struct SessionStats {
  std::uint64_t eventsDelivered = 0;
  std::uint64_t eventsLost = 0;
  std::uint64_t buffersWritten = 0;
  std::uint64_t bufferSizeKB = 0;
  SessionState state = SessionState::Stopped;
};

class EtwSession {
public:
  EtwSession() = default;
  ~EtwSession() { Stop(); }

  EtwSession(const EtwSession&) = delete;
  EtwSession& operator=(const EtwSession&) = delete;

  bool Start(const SessionConfig& config) {
    if (state_ == SessionState::Running) return true;

    config_ = config;

    ULONG bufferSize = sizeof(EVENT_TRACE_PROPERTIES) +
      (static_cast<ULONG>(config.name.size()) + 1) * sizeof(WCHAR) + sizeof(WCHAR);

    std::vector<BYTE> buffer(bufferSize, 0);
    auto* props = reinterpret_cast<EVENT_TRACE_PROPERTIES*>(buffer.data());

    props->Wnode.BufferSize = bufferSize;
    props->Wnode.Flags = WNODE_FLAG_TRACED_GUID;
    props->Wnode.ClientContext = 1;
    props->LogFileMode = config.realTime ?
      (EVENT_TRACE_REAL_TIME_MODE | EVENT_TRACE_SYSTEM_LOGGER_MODE) : 0;
    props->BufferSize = static_cast<ULONG>(config.bufferSizeKB);
    props->MinimumBuffers = static_cast<ULONG>(config.minBuffers);
    props->MaximumBuffers = static_cast<ULONG>(config.maxBuffers);
    props->FlushTimer = static_cast<ULONG>(config.flushTimerSec);
    props->LoggerNameOffset = sizeof(EVENT_TRACE_PROPERTIES);

    ULONG result = StartTraceW(&sessionHandle_, config.name.c_str(), props);

    if (result == ERROR_SUCCESS || result == ERROR_ALREADY_EXISTS) {
      state_ = SessionState::Running;
      stats_.state = SessionState::Running;
      stats_.bufferSizeKB = config.bufferSizeKB;
      return true;
    }

    state_ = SessionState::Error;
    lastError_ = result;
    return false;
  }

  bool Stop() {
    if (state_ != SessionState::Running) return true;

    ULONG bufferSize = sizeof(EVENT_TRACE_PROPERTIES) +
      (static_cast<ULONG>(config_.name.size()) + 1) * sizeof(WCHAR) + sizeof(WCHAR);

    std::vector<BYTE> buffer(bufferSize, 0);
    auto* props = reinterpret_cast<EVENT_TRACE_PROPERTIES*>(buffer.data());
    props->Wnode.BufferSize = bufferSize;
    props->LoggerNameOffset = sizeof(EVENT_TRACE_PROPERTIES);

    ULONG result = ControlTraceW(sessionHandle_, config_.name.c_str(),
      props, EVENT_TRACE_CONTROL_STOP);

    sessionHandle_ = 0;
    state_ = SessionState::Stopped;

    return result == ERROR_SUCCESS;
  }

  bool IsRunning() const { return state_ == SessionState::Running; }
  SessionState State() const { return state_; }
  const SessionConfig& Config() const { return config_; }
  const SessionStats& Stats() const { return stats_; }
  ULONG LastError() const { return lastError_; }
  TRACEHANDLE Handle() const { return sessionHandle_; }

private:
  TRACEHANDLE sessionHandle_ = 0;
  SessionState state_ = SessionState::Stopped;
  SessionConfig config_;
  SessionStats stats_;
  ULONG lastError_ = 0;
};

}