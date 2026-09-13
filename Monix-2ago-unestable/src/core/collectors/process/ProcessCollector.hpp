#pragma once

#include "ProcessTypes.hpp"
#include "ProcessIdentity.hpp"
#include "ProcessSnapshot.hpp"

#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>

namespace monix::collectors::proc {

using ProcessEventCallback = std::function<void(ProcessEventKind, ObservationOrigin, const ProcessInfo&)>;

struct ProcessCollectorConfig {
  std::uint32_t poll_interval_ms = 1000;
  bool track_suspended = false;
  bool capture_command_line = true;
  bool capture_working_directory = true;
  std::size_t max_tracked = 10000;

  static ProcessCollectorConfig defaults();
};

class ProcessCollector {
public:
  ProcessCollector();
  ~ProcessCollector();

  ProcessCollector(const ProcessCollector&) = delete;
  ProcessCollector& operator=(const ProcessCollector&) = delete;

  bool start(ProcessCollectorConfig config = ProcessCollectorConfig::defaults());
  bool stop();
  bool isRunning() const;

  void setCallback(ProcessEventCallback callback);

  ProcessSnapshot snapshot() const;
  std::size_t trackedCount() const;
  std::size_t eventsEmitted() const;
  std::size_t initialSnapshotCount() const;
  bool wasInInitialSnapshot(ProcessInstanceId id) const;

  static ProcessId currentPid();
  static ProcessId currentParentPid();

private:
  void pollLoop();
  void emitEvent(ProcessEventKind kind, ObservationOrigin origin, const ProcessInfo& info);
  void reconcile(const ProcessSnapshot& current, ObservationOrigin origin);

  ProcessCollectorConfig config_;
  ProcessEventCallback callback_;
  ProcessSnapshot previous_snapshot_;
  std::unordered_map<ProcessInstanceId, ProcessInfo> tracked_;
  std::unordered_set<ProcessInstanceId> initial_snapshot_ids_;
  std::unordered_set<ProcessId> suspended_pids_;

  mutable std::mutex mu_;
  std::thread poll_thread_;
  std::atomic<bool> running_{false};
  bool stopping_{false};
  std::condition_variable stop_cv_;
  std::atomic<std::size_t> events_emitted_{0};
};

}  // namespace monix::collectors::proc
