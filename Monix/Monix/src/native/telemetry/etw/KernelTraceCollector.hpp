#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <evntrace.h>
#include <evntcons.h>

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace monix::etw {

struct KernelEvent {
  WORD eventType = 0;
  DWORD processId = 0;
  DWORD threadId = 0;
  DWORD cpu = 0;
  const void* userData = nullptr;
  ULONG userDataLen = 0;
  std::uint64_t timestamp100ns = 0;
};

using KernelEventCallback = std::function<void(const KernelEvent&)>;

struct KernelTraceConfig {
  bool processThread = true;
  bool diskIO = true;
  bool networkTCPIP = true;
  bool fileIO = false;
  bool registry = false;
  bool imageLoad = false;
  bool contextSwitch = false;
  bool interruptDPC = false;

  UINT64 Keywords() const {
    UINT64 kw = 0;
    if (processThread) kw |= EVENT_TRACE_FLAG_PROCESS | EVENT_TRACE_FLAG_THREAD;
    if (diskIO) kw |= EVENT_TRACE_FLAG_DISK_IO | EVENT_TRACE_FLAG_DISK_FILE_IO;
    if (networkTCPIP) kw |= EVENT_TRACE_FLAG_NETWORK_TCPIP;
    if (fileIO) kw |= EVENT_TRACE_FLAG_DISK_FILE_IO;
    if (registry) kw |= EVENT_TRACE_FLAG_REGISTRY;
    if (imageLoad) kw |= EVENT_TRACE_FLAG_IMAGE_LOAD;
    if (contextSwitch) kw |= EVENT_TRACE_FLAG_CSWITCH;
    if (interruptDPC) kw |= EVENT_TRACE_FLAG_INTERRUPT | EVENT_TRACE_FLAG_DPC;
    return kw;
  }
};

class KernelTraceCollector {
public:
  KernelTraceCollector() = default;

  bool Start(const KernelTraceConfig& config = KernelTraceConfig{}) {
    config_ = config;
    return StartKernelSession();
  }

  bool Stop() {
    if (!running_) return true;

    ULONG bufferSize = sizeof(EVENT_TRACE_PROPERTIES) +
      (static_cast<ULONG>(kSessionName.size()) + 1) * sizeof(WCHAR) + sizeof(WCHAR);

    std::vector<BYTE> buffer(bufferSize, 0);
    auto* props = reinterpret_cast<EVENT_TRACE_PROPERTIES*>(buffer.data());
    props->Wnode.BufferSize = bufferSize;
    props->LoggerNameOffset = sizeof(EVENT_TRACE_PROPERTIES);

    ULONG result = ControlTraceW(sessionHandle_, kSessionName.c_str(),
      props, EVENT_TRACE_CONTROL_STOP);

    running_ = false;
    sessionHandle_ = 0;

    return result == ERROR_SUCCESS;
  }

  bool IsRunning() const { return running_; }

  void OnEvent(KernelEventCallback cb) { callback_ = std::move(cb); }

  bool ProcessEvent(PEVENT_RECORD record) {
    if (!record || !callback_) return false;

    KernelEvent evt;
    evt.timestamp100ns = record->EventHeader.TimeStamp.QuadPart;
    evt.processId = record->EventHeader.ProcessId;
    evt.threadId = record->EventHeader.ThreadId;
    evt.cpu = record->BufferContext.ProcessorNumber;
    evt.userData = record->UserData;
    evt.userDataLen = record->UserDataLength;
    evt.eventType = record->EventHeader.EventDescriptor.Opcode;

    callback_(evt);
    eventsProcessed_++;

    return true;
  }

  std::uint64_t EventsProcessed() const { return eventsProcessed_; }

private:
  bool StartKernelSession() {
    ULONG bufferSize = sizeof(EVENT_TRACE_PROPERTIES) +
      (static_cast<ULONG>(kSessionName.size()) + 1) * sizeof(WCHAR) + sizeof(WCHAR);

    std::vector<BYTE> buffer(bufferSize, 0);
    auto* props = reinterpret_cast<EVENT_TRACE_PROPERTIES*>(buffer.data());

    props->Wnode.BufferSize = bufferSize;
    props->Wnode.Flags = WNODE_FLAG_TRACED_GUID;
    props->Wnode.ClientContext = 1;
    props->EnableFlags = config_.Keywords();
    props->LogFileMode = EVENT_TRACE_SYSTEM_LOGGER_MODE | EVENT_TRACE_REAL_TIME_MODE;
    props->LoggerNameOffset = sizeof(EVENT_TRACE_PROPERTIES);

    ULONG result = StartTraceW(&sessionHandle_, kSessionName.c_str(), props);

    if (result == ERROR_SUCCESS || result == ERROR_ALREADY_EXISTS) {
      running_ = true;
      return true;
    }

    lastError_ = result;
    return false;
  }

  static inline const std::wstring kSessionName = L"MonixKernelTrace";

  KernelTraceConfig config_;
  TRACEHANDLE sessionHandle_ = 0;
  bool running_ = false;
  std::uint64_t eventsProcessed_ = 0;
  ULONG lastError_ = 0;
  KernelEventCallback callback_;
};

}