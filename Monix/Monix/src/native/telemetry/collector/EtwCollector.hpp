#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <evntrace.h>
#include <evntcons.h>
#include <evntprov.h>
#pragma comment(lib, "advapi32.lib")

#include <cstdint>
#include <functional>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>

namespace monix::telemetry {

struct EtwProcessEvent {
  std::uint32_t pid = 0;
  std::uint32_t parentPid = 0;
  std::wstring imageName;
  bool isCreate = true;
  std::uint64_t timestampNs = 0;
};

using EtwEventCallback = std::function<void(const EtwProcessEvent&)>;

class EtwCollector {
public:
  EtwCollector() = default;
  ~EtwCollector() { Stop(); }

  bool Start(EtwEventCallback callback);
  void Stop();
  bool IsRunning() const { return running_; }
  void OnEventRecord(PEVENT_RECORD record);

private:
  void ConsumerLoop();

  std::atomic<bool> running_{false};
  std::thread consumerThread_;
  EtwEventCallback callback_;
  std::mutex callbackMutex_;
  TRACEHANDLE sessionHandle_ = 0;
  TRACEHANDLE traceLogHandle_ = 0;
  EVENT_TRACE_PROPERTIES* properties_ = nullptr;
};

} // namespace monix::telemetry
