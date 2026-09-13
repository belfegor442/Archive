#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "../Snapshot.hpp"
#include "../contract/EntityIdentity.hpp"
#include "ChangeSet.hpp"

namespace monix::telemetry {

enum class EntityLifecycle {
  Created,
  Running,
  Changed,
  Terminated
};

struct TrackedProcess {
  ProcessInfo info;
  ProcessDelta delta;
  EntityLifecycle lifecycle = EntityLifecycle::Created;
  std::uint64_t firstSeenNs = 0;
  std::uint64_t lastSeenNs = 0;
  std::uint64_t lastChangedNs = 0;
  int unchangedCount = 0;
};

struct TrackedThread {
  std::uint32_t tid = 0;
  std::uint32_t ownerPid = 0;
  EntityLifecycle lifecycle = EntityLifecycle::Created;
  std::uint64_t firstSeenNs = 0;
  std::uint64_t lastSeenNs = 0;
};

struct TrackedDriver {
  std::wstring name;
  EntityLifecycle lifecycle = EntityLifecycle::Created;
  std::uint64_t firstSeenNs = 0;
  std::uint64_t lastSeenNs = 0;
};

class EntityTracker {
public:
  void UpdateProcesses(const std::vector<ProcessInfo>& current,
                       std::uint64_t timestampNs,
                       ChangeSet& out);

  void UpdateDrivers(const std::set<std::wstring>& currentDriverNames,
                     std::uint64_t timestampNs,
                     ChangeSet& out);

  const TrackedProcess* FindProcess(std::int32_t pid) const;
  std::size_t ProcessCount() const { return processes_.size(); }

  const auto& Processes() const { return processes_; }
  const auto& Drivers() const { return drivers_; }

private:
  std::unordered_map<std::int32_t, TrackedProcess> processes_;
  std::unordered_map<std::wstring, TrackedDriver> drivers_;
};

}
