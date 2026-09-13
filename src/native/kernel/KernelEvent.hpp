#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

namespace monix {
namespace kernel {

enum class EventSubsystem {
  Log,
  Telemetry,
  Sensors,
  Devices,
  Storage,
  Memory,
  Process,
  Threads,
  Security,
  Error,
  Network,
  Kernel,
  Graphics,
  Audio,
  Configuration,
  Scheduler,
  IPC
};

enum class EventSeverity {
  Info,
  Warning,
  Error,
  Fatal
};

struct KernelEvent {
  std::uint32_t id = 0;
  EventSubsystem subsystem = EventSubsystem::Kernel;
  EventSeverity severity = EventSeverity::Info;
  const char* name = nullptr;
  const char* description = nullptr;
};

class KernelEventEngine {
public:
  KernelEventEngine() { events_.reserve(256); }

  std::uint32_t Emit(EventSubsystem subsystem, EventSeverity severity,
                     const char* name, const char* description = nullptr) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::uint32_t id = nextId_++;
    events_.push_back({ id, subsystem, severity, name, description });
    return id;
  }

  std::uint32_t NextId() const { return nextId_; }
  std::size_t Count() const { return events_.size(); }
  const KernelEvent& Last() const {
    static const KernelEvent empty{};
    if (events_.empty()) return empty;
    return events_.back();
  }
  const std::vector<KernelEvent>& Events() const { return events_; }

  void Clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    events_.clear();
    nextId_ = 1;
  }

  static const char* SubsystemName(EventSubsystem s) {
    switch (s) {
      case EventSubsystem::Log: return "LOG";
      case EventSubsystem::Telemetry: return "TELEMETRY";
      case EventSubsystem::Sensors: return "SENSORS";
      case EventSubsystem::Devices: return "DEVICES";
      case EventSubsystem::Storage: return "STORAGE";
      case EventSubsystem::Memory: return "MEMORY";
      case EventSubsystem::Process: return "PROCESS";
      case EventSubsystem::Threads: return "THREADS";
      case EventSubsystem::Security: return "SECURITY";
      case EventSubsystem::Error: return "ERROR";
      case EventSubsystem::Network: return "NETWORK";
      case EventSubsystem::Kernel: return "KERNEL";
      case EventSubsystem::Graphics: return "GRAPHICS";
      case EventSubsystem::Audio: return "AUDIO";
      case EventSubsystem::Configuration: return "CONFIGURATION";
      case EventSubsystem::Scheduler: return "SCHEDULER";
      case EventSubsystem::IPC: return "IPC";
    }
    return "UNKNOWN";
  }

  static const char* SeverityName(EventSeverity s) {
    switch (s) {
      case EventSeverity::Info: return "INFO";
      case EventSeverity::Warning: return "WARNING";
      case EventSeverity::Error: return "ERROR";
      case EventSeverity::Fatal: return "FATAL";
    }
    return "UNKNOWN";
  }

private:
  std::uint32_t nextId_ = 1;
  std::vector<KernelEvent> events_;
  mutable std::mutex mutex_;
};

}
}
