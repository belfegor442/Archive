#include "EntityTracker.hpp"

#include <algorithm>

namespace monix::telemetry {

void EntityTracker::UpdateProcesses(const std::vector<ProcessInfo>& current,
                                    std::uint64_t timestampNs,
                                    ChangeSet& out) {
  std::unordered_map<std::int32_t, bool> seen;

  for (const auto& proc : current) {
    seen[proc.pid] = true;
    auto it = processes_.find(proc.pid);
    if (it == processes_.end()) {
      TrackedProcess tp;
      tp.info = proc;
      tp.lifecycle = EntityLifecycle::Created;
      tp.firstSeenNs = timestampNs;
      tp.lastSeenNs = timestampNs;
      processes_[proc.pid] = tp;
      out.AddCreated(EntityIdentity::Process(proc.pid, proc.createTime100ns),
                     "process", timestampNs);
    } else {
      auto& tracked = it->second;
      bool changed = (tracked.info.cpuPct != proc.cpuPct) ||
                     (tracked.info.ramBytes != proc.ramBytes) ||
                     (tracked.info.status != proc.status);
      tracked.lastSeenNs = timestampNs;
      if (changed) {
        tracked.lifecycle = EntityLifecycle::Changed;
        tracked.lastChangedNs = timestampNs;
        tracked.unchangedCount = 0;
        out.AddModified(EntityIdentity::Process(proc.pid, proc.createTime100ns),
                        "process", timestampNs);
      } else {
        tracked.unchangedCount++;
      }
      tracked.info = proc;
    }
  }

  for (auto it = processes_.begin(); it != processes_.end(); ) {
    if (!seen.count(it->first)) {
      out.AddTerminated(
          EntityIdentity::Process(it->first, it->second.info.createTime100ns),
          "process", timestampNs);
      it = processes_.erase(it);
    } else {
      ++it;
    }
  }
}

void EntityTracker::UpdateDrivers(const std::set<std::wstring>& currentDriverNames,
                                  std::uint64_t timestampNs,
                                  ChangeSet& out) {
  std::unordered_map<std::wstring, bool> seen;

  for (const auto& name : currentDriverNames) {
    seen[name] = true;
    auto it = drivers_.find(name);
    if (it == drivers_.end()) {
      TrackedDriver td;
      td.name = name;
      td.lifecycle = EntityLifecycle::Created;
      td.firstSeenNs = timestampNs;
      td.lastSeenNs = timestampNs;
      drivers_[name] = td;
      out.AddCreated(EntityIdentity::Driver(name), "driver", timestampNs);
    } else {
      it->second.lastSeenNs = timestampNs;
    }
  }

  for (auto it = drivers_.begin(); it != drivers_.end(); ) {
    if (!seen.count(it->first)) {
      out.AddTerminated(EntityIdentity::Driver(it->first), "driver", timestampNs);
      it = drivers_.erase(it);
    } else {
      ++it;
    }
  }
}

const TrackedProcess* EntityTracker::FindProcess(std::int32_t pid) const {
  auto it = processes_.find(pid);
  return it != processes_.end() ? &it->second : nullptr;
}

}
