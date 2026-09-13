#include "ProcessCollector.hpp"

#include <chrono>
#include <algorithm>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <tlhelp32.h>

namespace monix::collectors::proc {

ProcessCollectorConfig ProcessCollectorConfig::defaults() {
  ProcessCollectorConfig cfg;
  cfg.poll_interval_ms = 1000;
  cfg.track_suspended = false;
  cfg.capture_command_line = true;
  cfg.capture_working_directory = true;
  cfg.max_tracked = 10000;
  return cfg;
}

ProcessCollector::ProcessCollector() = default;

ProcessCollector::~ProcessCollector() {
  stop();
}

bool ProcessCollector::start(ProcessCollectorConfig config) {
  if (running_) return false;
  config_ = std::move(config);
  running_ = true;
  stopping_ = false;
  events_emitted_ = 0;
  initial_snapshot_ids_.clear();

  ProcessSnapshot snap;
  if (snap.capture()) {
    previous_snapshot_ = snap;
    for (const auto& info : snap.processes()) {
      tracked_[info.instance_id] = info;
      initial_snapshot_ids_.insert(info.instance_id);
    }
  }

  poll_thread_ = std::thread(&ProcessCollector::pollLoop, this);
  return true;
}

bool ProcessCollector::stop() {
  if (!running_) return false;
  {
    std::lock_guard<std::mutex> lock(mu_);
    stopping_ = true;
  }
  running_ = false;
  stop_cv_.notify_all();

  if (poll_thread_.joinable()) {
    poll_thread_.join();
  }

  tracked_.clear();
  initial_snapshot_ids_.clear();
  suspended_pids_.clear();
  return true;
}

bool ProcessCollector::isRunning() const {
  return running_;
}

void ProcessCollector::setCallback(ProcessEventCallback callback) {
  callback_ = std::move(callback);
}

ProcessSnapshot ProcessCollector::snapshot() const {
  std::lock_guard<std::mutex> lock(mu_);
  ProcessSnapshot snap;
  return previous_snapshot_;
}

std::size_t ProcessCollector::trackedCount() const {
  return tracked_.size();
}

std::size_t ProcessCollector::eventsEmitted() const {
  return events_emitted_;
}

std::size_t ProcessCollector::initialSnapshotCount() const {
  return initial_snapshot_ids_.size();
}

bool ProcessCollector::wasInInitialSnapshot(ProcessInstanceId id) const {
  return initial_snapshot_ids_.find(id) != initial_snapshot_ids_.end();
}

ProcessId ProcessCollector::currentPid() {
  return static_cast<ProcessId>(GetCurrentProcessId());
}

ProcessId ProcessCollector::currentParentPid() {
  HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (snap == INVALID_HANDLE_VALUE) return 0;

  DWORD myPid = GetCurrentProcessId();
  DWORD parentPid = 0;

  PROCESSENTRY32W pe;
  pe.dwSize = sizeof(pe);

  if (Process32FirstW(snap, &pe)) {
    do {
      if (pe.th32ProcessID == myPid) {
        parentPid = pe.th32ParentProcessID;
        break;
      }
    } while (Process32NextW(snap, &pe));
  }

  CloseHandle(snap);
  return parentPid;
}

void ProcessCollector::pollLoop() {
  bool firstPoll = true;
  while (running_) {
    ProcessSnapshot current;
    if (current.capture()) {
      std::lock_guard<std::mutex> lock(mu_);
      ObservationOrigin origin = firstPoll
        ? ObservationOrigin::InitialSnapshot
        : ObservationOrigin::Polling;
      reconcile(current, origin);
      previous_snapshot_ = current;
      firstPoll = false;
    }

    std::unique_lock<std::mutex> lock(mu_);
    stop_cv_.wait_for(lock, std::chrono::milliseconds(config_.poll_interval_ms),
                      [this] { return stopping_; });
  }
}

void ProcessCollector::emitEvent(ProcessEventKind kind, ObservationOrigin origin, const ProcessInfo& info) {
  events_emitted_++;
  if (callback_) {
    callback_(kind, origin, info);
  }
}

void ProcessCollector::reconcile(const ProcessSnapshot& current, ObservationOrigin origin) {
  for (const auto& info : current.processes()) {
    auto it = tracked_.find(info.instance_id);
    if (it == tracked_.end()) {
      if (tracked_.size() < config_.max_tracked) {
        tracked_[info.instance_id] = info;
        if (origin == ObservationOrigin::InitialSnapshot) {
          initial_snapshot_ids_.insert(info.instance_id);
        } else {
          emitEvent(ProcessEventKind::Started, origin, info);
        }
      }
    } else {
      if (!info.is_alive() && it->second.is_alive()) {
        ProcessInfo terminated = info;
        emitEvent(ProcessEventKind::Terminated, origin, terminated);
        tracked_.erase(it);
        initial_snapshot_ids_.erase(info.instance_id);
      } else if (config_.track_suspended) {
        bool was_suspended = suspended_pids_.count(info.pid) > 0;
        bool is_suspended = false;

        HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, info.pid);
        if (hProc) {
          DWORD exitCode = 0;
          if (GetExitCodeProcess(hProc, &exitCode)) {
            is_suspended = (exitCode == 259);
          }
          CloseHandle(hProc);
        }

        if (is_suspended && !was_suspended) {
          suspended_pids_.insert(info.pid);
          emitEvent(ProcessEventKind::Suspended, origin, info);
        } else if (!is_suspended && was_suspended) {
          suspended_pids_.erase(info.pid);
          emitEvent(ProcessEventKind::Resumed, origin, info);
        }
      }
    }
  }

  auto removed = current.removedPids(previous_snapshot_);
  for (ProcessId pid : removed) {
    auto it = tracked_.begin();
    while (it != tracked_.end()) {
      if (it->second.pid == pid) {
        ProcessInfo terminated = it->second;
        terminated.exit_time_ms = current.captureTimeMs();
        emitEvent(ProcessEventKind::Terminated, origin, terminated);
        suspended_pids_.erase(pid);
        initial_snapshot_ids_.erase(it->first);
        it = tracked_.erase(it);
      } else {
        ++it;
      }
    }
  }
}

}  // namespace monix::collectors::proc
