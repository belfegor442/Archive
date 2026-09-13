#include "ProcessTriageRule.hpp"

#include "../../telemetry/Snapshot.hpp"

#include <algorithm>

namespace monix {

void ProcessTriageRule::Evaluate(const Snapshot& current,
                                 const Snapshot* /*previous*/,
                                 std::vector<ScramFinding>& findings) {
  if (!current.processes.empty()) {
    const auto top = std::max_element(current.processes.begin(), current.processes.end(),
      [](const ProcessInfo& left, const ProcessInfo& right) {
        const double leftScore = left.cpuPct * 2.0 + (left.ramBytes / 1048576.0) * 0.02 + left.gpuPct * 1.5;
        const double rightScore = right.cpuPct * 2.0 + (right.ramBytes / 1048576.0) * 0.02 + right.gpuPct * 1.5;
        return leftScore < rightScore;
      });
    if (top != current.processes.end()) {
      if (top->cpuPct >= 25.0 || top->ramBytes >= 2ull * 1024ull * 1024ull * 1024ull) {
        auto it = reported_.find(top->pid);
        bool shouldReport = false;
        if (it == reported_.end()) {
          shouldReport = true;
        } else {
          const double cpuDelta = top->cpuPct - it->second.lastReportedCpuPct;
          const std::int64_t ramDelta = static_cast<std::int64_t>(top->ramBytes) - static_cast<std::int64_t>(it->second.lastReportedRamBytes);
          if (cpuDelta > 15.0 || ramDelta > 512 * 1024 * 1024) {
            shouldReport = true;
          }
        }
        if (shouldReport) {
          const double ramMb = top->ramBytes / 1048576.0;
          const double compositeScore = top->cpuPct * 2.0 + ramMb * 0.02 + top->gpuPct * 1.5;
          findings.push_back({
            L"Top process resource consumer: " + top->name,
            L"Process " + top->name + L" (PID " + std::to_wstring(top->pid) + L") consuming significant resources.",
            L"top_process=" + top->name +
            L" pid=" + std::to_wstring(top->pid) +
            L" cpu=" + std::to_wstring(static_cast<int>(top->cpuPct)) + L"%" +
            L" ram=" + std::to_wstring(static_cast<int>(ramMb)) + L"MB" +
            L" gpu=" + std::to_wstring(static_cast<int>(top->gpuPct)) + L"%" +
            L" composite=" + std::to_wstring(static_cast<int>(compositeScore)) +
            L" system_cpu=" + std::to_wstring(static_cast<int>(current.cpuPct)) + L"%",
            8
          });
          reported_[top->pid] = { top->cpuPct, top->ramBytes, (it != reported_.end() ? it->second.reportCount + 1 : 1) };
        }
      }
    }
  }

  for (auto it = reported_.begin(); it != reported_.end(); ) {
    bool found = false;
    for (const auto& p : current.processes) {
      if (p.pid == it->first) { found = true; break; }
    }
    if (!found) {
      it = reported_.erase(it);
    } else {
      ++it;
    }
  }
}

} // namespace monix
