#include "EtwCollector.hpp"

static void __stdcall EtwCallbackStdCall(PEVENT_RECORD record, void* ctx) {
  if (auto* self = static_cast<monix::telemetry::EtwCollector*>(ctx)) {
    self->OnEventRecord(record);
  }
}

namespace monix::telemetry {

bool EtwCollector::Start(EtwEventCallback callback) {
  if (running_) return true;
  callback_ = std::move(callback);
  running_ = true;
  consumerThread_ = std::thread(&EtwCollector::ConsumerLoop, this);
  return true;
}

void EtwCollector::Stop() {
  running_ = false;
  if (sessionHandle_ != 0) {
    ControlTraceW(sessionHandle_, nullptr, properties_, EVENT_TRACE_CONTROL_STOP);
    sessionHandle_ = 0;
  }
  if (consumerThread_.joinable()) {
    consumerThread_.join();
  }
}

void EtwCollector::OnEventRecord(PEVENT_RECORD record) {
  if (!callback_ || !running_) return;

  EtwProcessEvent evt;
  evt.timestampNs = static_cast<std::uint64_t>(record->EventHeader.TimeStamp.QuadPart) * 100ULL;

  WORD eventId = record->EventHeader.EventDescriptor.Id;

  if (eventId == 1 && record->UserDataLength >= 4) {
    evt.isCreate = true;
    evt.pid = *reinterpret_cast<std::uint32_t*>(record->UserData);
    if (record->UserDataLength >= 8) {
      evt.parentPid = *reinterpret_cast<std::uint32_t*>(static_cast<std::uint8_t*>(record->UserData) + 4);
    }
    std::lock_guard<std::mutex> lock(callbackMutex_);
    callback_(evt);
  } else if (eventId == 2 && record->UserDataLength >= 4) {
    evt.isCreate = false;
    evt.pid = *reinterpret_cast<std::uint32_t*>(record->UserData);
    std::lock_guard<std::mutex> lock(callbackMutex_);
    callback_(evt);
  }
}

void EtwCollector::ConsumerLoop() {
  ULONG bufSize = sizeof(EVENT_TRACE_PROPERTIES) + sizeof(WCHAR) * 256;
  properties_ = reinterpret_cast<EVENT_TRACE_PROPERTIES*>(new char[bufSize]);
  ZeroMemory(properties_, bufSize);
  properties_->Wnode.BufferSize = bufSize;
  properties_->Wnode.Flags = WNODE_FLAG_TRACED_GUID;
  properties_->Wnode.ClientContext = 1;
  properties_->LogFileMode = EVENT_TRACE_REAL_TIME_MODE | EVENT_TRACE_SYSTEM_LOGGER_MODE;
  properties_->LoggerNameOffset = sizeof(EVENT_TRACE_PROPERTIES);
  properties_->EnableFlags = EVENT_TRACE_FLAG_PROCESS;

  ULONG status = StartTraceW(&sessionHandle_, L"MonixETW", properties_);
  if (status == ERROR_ALREADY_EXISTS) {
    ControlTraceW(0, L"MonixETW", properties_, EVENT_TRACE_CONTROL_STOP);
    status = StartTraceW(&sessionHandle_, L"MonixETW", properties_);
  }
  if (status != ERROR_SUCCESS) {
    delete[] reinterpret_cast<char*>(properties_);
    properties_ = nullptr;
    running_ = false;
    return;
  }

  EVENT_TRACE_LOGFILEW logFile = {};
  logFile.LoggerName = const_cast<WCHAR*>(L"MonixETW");
  logFile.ProcessTraceMode = PROCESS_TRACE_MODE_REAL_TIME | PROCESS_TRACE_MODE_EVENT_RECORD;
  logFile.Context = this;
  logFile.EventRecordCallback = reinterpret_cast<PEVENT_RECORD_CALLBACK>(
    reinterpret_cast<void*>(&EtwCallbackStdCall));

  traceLogHandle_ = OpenTraceW(&logFile);
  if (traceLogHandle_ == 0) {
    ControlTraceW(sessionHandle_, nullptr, properties_, EVENT_TRACE_CONTROL_STOP);
    sessionHandle_ = 0;
    delete[] reinterpret_cast<char*>(properties_);
    properties_ = nullptr;
    running_ = false;
    return;
  }

  ProcessTrace(&traceLogHandle_, 1, nullptr, nullptr);

  CloseTrace(traceLogHandle_);
  traceLogHandle_ = 0;
  if (sessionHandle_ != 0) {
    ControlTraceW(sessionHandle_, nullptr, properties_, EVENT_TRACE_CONTROL_STOP);
    sessionHandle_ = 0;
  }
  delete[] reinterpret_cast<char*>(properties_);
  properties_ = nullptr;
  running_ = false;
}

} // namespace monix::telemetry
