#pragma once

#include "ProcessTypes.hpp"
#include "ProcessIdentity.hpp"

#include <vector>
#include <unordered_map>
#include <cstdint>

namespace monix::collectors::proc {

struct ProcessSnapshotEntry {
  ProcessInfo info;
  bool is_new = false;
};

class ProcessSnapshot {
public:
  ProcessSnapshot();

  bool capture();
  bool captureSingle(ProcessId pid);

  std::vector<ProcessInfo> processes() const;
  std::size_t processCount() const;
  bool contains(ProcessInstanceId id) const;
  bool containsPid(ProcessId pid) const;
  const ProcessInfo* find(ProcessInstanceId id) const;
  const ProcessInfo* findByPid(ProcessId pid) const;

  std::vector<ProcessInfo> diffsFrom(const ProcessSnapshot& previous) const;
  std::vector<ProcessId> newPids(const ProcessSnapshot& previous) const;
  std::vector<ProcessId> removedPids(const ProcessSnapshot& previous) const;

  void clear();
  std::int64_t captureTimeMs() const;

  static ProcessInfo selfInfo();
  static ProcessInfo currentProcessInfo();

private:
  static std::int64_t nowMs();
  static std::string extractBaseName(const std::string& path);
  static ProcessArchitecture detectArch();

  std::unordered_map<ProcessInstanceId, ProcessInfo> entries_;
  std::unordered_map<ProcessId, ProcessInstanceId> pid_to_id_;
  std::int64_t capture_time_ms_ = 0;
};

}  // namespace monix::collectors::proc
