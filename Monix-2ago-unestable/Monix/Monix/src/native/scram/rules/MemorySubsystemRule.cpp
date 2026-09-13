#include "MemorySubsystemRule.hpp"

#include "../../telemetry/Snapshot.hpp"
#include "../../core/TextUtils.hpp"

namespace monix {

void MemorySubsystemRule::Evaluate(const Snapshot& current,
                                   const Snapshot* previous,
                                   std::vector<ScramFinding>& findings) {
  if (!previous) return;
  const auto& prev = *previous;

  if (prev.ramTotalBytes > 0 && current.ramTotalBytes != prev.ramTotalBytes) {
    findings.push_back({
      L"Physical memory topology changed.",
      L"Total physical memory differs between samples.",
      (L"RAM total: " + std::to_wstring(prev.ramTotalBytes / (1024*1024))
        + L" -> " + std::to_wstring(current.ramTotalBytes / (1024*1024)) + L" MB"),
      30
    });
  }

  if (current.ramAvailBytes > 0 && current.ramAvailBytes < current.ramTotalBytes * 0.05) {
    findings.push_back({
      L"Available memory critically depleted.",
      L"Less than 5% of physical RAM available.",
      (L"Available: " + std::to_wstring(current.ramAvailBytes / (1024*1024))
        + L" MB / " + std::to_wstring(current.ramTotalBytes / (1024*1024)) + L" MB"),
      22
    });
  }

  const int64_t commitDelta = (int64_t)current.commitUsedBytes - (int64_t)prev.commitUsedBytes;
  if (commitDelta > 512 * 1024 * 1024) {
    findings.push_back({
      L"Commit charge surge detected.",
      L"Committed memory increased rapidly.",
      (L"Commit: +" + std::to_wstring(commitDelta / (1024*1024)) + L" MB ("
        + std::to_wstring(current.commitUsedBytes / (1024*1024)) + L"/"
        + std::to_wstring(current.commitLimitBytes / (1024*1024)) + L" MB)"),
      12
    });
  }

  if (current.commitPressurePct > 90.0) {
    findings.push_back({
      (L"Commit limit pressure at " + std::to_wstring((int)current.commitPressurePct) + L"%"),
      L"Virtual address space approaching the commit limit.",
      (L"Commit: " + std::to_wstring(current.commitUsedBytes / (1024*1024))
        + L" / " + std::to_wstring(current.commitLimitBytes / (1024*1024)) + L" MB"),
      14
    });
  } else if (current.commitPressurePct > 80.0) {
    findings.push_back({
      (L"Commit pressure elevated at " + std::to_wstring((int)current.commitPressurePct) + L"%"),
      L"Commit usage moderately high.",
      (L"Commit pressure elevated: " + std::to_wstring((int)current.commitPressurePct) + L"%"),
      6
    });
  }

  if (prev.workingSetTotalBytes > 0) {
    const int64_t wsDelta = (int64_t)current.workingSetTotalBytes - (int64_t)prev.workingSetTotalBytes;
    if (wsDelta > 256 * 1024 * 1024) {
      findings.push_back({
        L"System working set growth detected.",
        L"Working set expanding beyond reclaim capacity.",
        (L"Working set: +" + std::to_wstring(wsDelta / (1024*1024)) + L" MB ("
          + std::to_wstring(current.workingSetTotalBytes / (1024*1024)) + L" MB)"),
        10
      });
    } else if (wsDelta < -256 * 1024 * 1024) {
      findings.push_back({
        L"System working set trimmed.",
        L"Working set reclaimed significant memory.",
        (L"Working set trimmed: " + std::to_wstring(-wsDelta / (1024*1024)) + L" MB reclaimed"),
        4
      });
    }
  }

  if (current.pageFaultsDelta > 5000) {
    findings.push_back({
      (L"Page fault rate spike: " + std::to_wstring(current.pageFaultsDelta) + L" faults"),
      L"Excessive paging activity degrades performance.",
      (L"Page faults: " + std::to_wstring(current.pageFaultsDelta) + L" per sample"),
      14
    });
  } else if (current.pageFaultsDelta > 2000) {
    findings.push_back({
      L"Elevated page faults detected.",
      L"Page fault rate moderately high.",
      (L"Elevated page faults: " + std::to_wstring(current.pageFaultsDelta)),
      6
    });
  }

  if (current.hardPageFaultsDelta > 100) {
    findings.push_back({
      (L"Hard page fault spike: " + std::to_wstring(current.hardPageFaultsDelta) + L" faults"),
      L"Disk-backed page I/O causing severe latency.",
      (L"Hard page faults: " + std::to_wstring(current.hardPageFaultsDelta) + L" per sample"),
      18
    });
  }

  if (current.modifiedListBytes > 128 * 1024 * 1024) {
    findings.push_back({
      L"Modified page list growing.",
      L"Excessive modified pages pending writeback.",
      (L"Modified page list growing: " + std::to_wstring(current.modifiedListBytes / (1024*1024)) + L" MB pending writeback"),
      8
    });
  }

  const uint64_t fragBytes = current.freeListBytes + current.zeroListBytes;
  if (prev.freeListBytes > 0) {
    const uint64_t prevFrag = prev.freeListBytes + prev.zeroListBytes;
    if (fragBytes > prevFrag * 1.5 && fragBytes > 64 * 1024 * 1024) {
      findings.push_back({
        L"Memory fragmentation growing.",
        L"Free and zero page lists indicate fragmentation.",
        (L"Memory fragmentation growing: " + std::to_wstring(fragBytes / (1024*1024)) + L" MB free/zero"),
        8
      });
    }
  }

  if (current.pagefilePctUsed > 90.0) {
    findings.push_back({
      (L"Pagefile saturation at " + std::to_wstring((int)current.pagefilePctUsed) + L"%"),
      L"Swap space nearly exhausted.",
      (L"Pagefile: " + std::to_wstring(current.pageFileUsedBytes / (1024*1024))
        + L" / " + std::to_wstring(current.pageFileTotalBytes / (1024*1024)) + L" MB"),
      16
    });
  } else if (current.pagefilePctUsed > 75.0) {
    findings.push_back({
      (L"Pagefile usage elevated at " + std::to_wstring((int)current.pagefilePctUsed) + L"%"),
      L"Pagefile usage moderately high.",
      (L"Pagefile usage elevated: " + std::to_wstring((int)current.pagefilePctUsed) + L"%"),
      6
    });
  }

  const int64_t pfDelta = (int64_t)current.pageFileUsedBytes - (int64_t)prev.pageFileUsedBytes;
  if (pfDelta > 256 * 1024 * 1024) {
    findings.push_back({
      L"Pagefile growth detected.",
      L"Pagefile usage increased significantly.",
      (L"Pagefile growth: +" + std::to_wstring(pfDelta / (1024*1024)) + L" MB"),
      8
    });
  }

  if (current.standbyListBytes > 0 && current.standbyListBytes < 64 * 1024 * 1024) {
    findings.push_back({
      L"Standby list depleted.",
      L"Standby list below minimum threshold.",
      (L"Standby list depleted: " + std::to_wstring(current.standbyListBytes / (1024*1024)) + L" MB"),
      10
    });
  }

  if (prev.kernelPoolNonpagedBytes > 0) {
    const int64_t poolDelta = (int64_t)current.kernelPoolNonpagedBytes - (int64_t)prev.kernelPoolNonpagedBytes;
    if (poolDelta > 16 * 1024 * 1024) {
      findings.push_back({
        (L"Nonpaged pool growth: +" + std::to_wstring(poolDelta / (1024*1024)) + L" MB"),
        L"Kernel nonpaged pool may be leaking.",
        (L"Nonpaged pool: " + std::to_wstring(current.kernelPoolNonpagedBytes / (1024*1024)) + L" MB"),
        12
      });
    }
  }

  if (prev.kernelPoolPagedBytes > 0) {
    const int64_t poolDelta = (int64_t)current.kernelPoolPagedBytes - (int64_t)prev.kernelPoolPagedBytes;
    if (poolDelta > 32 * 1024 * 1024) {
      findings.push_back({
        L"Paged pool growth detected.",
        L"Kernel paged pool increased significantly.",
        (L"Paged pool growth: +" + std::to_wstring(poolDelta / (1024*1024)) + L" MB ("
          + std::to_wstring(current.kernelPoolPagedBytes / (1024*1024)) + L" MB)"),
        8
      });
    }
  }

  if (!current.processes.empty() && !prev.processes.empty()) {
    uint64_t currentTotalWs = 0;
    uint64_t prevTotalWs = 0;
    for (const auto& p : current.processes) currentTotalWs += p.ramBytes;
    for (const auto& p : prev.processes) prevTotalWs += p.ramBytes;
    if (currentTotalWs > prevTotalWs * 1.1 && currentTotalWs - prevTotalWs > 512 * 1024 * 1024) {
      findings.push_back({
        L"Aggregate working set surge detected.",
        L"Total process working sets increased by over 512 MB.",
        (L"Aggregate working set surge: +"
          + std::to_wstring((currentTotalWs - prevTotalWs) / (1024*1024)) + L" MB across all processes"),
        10
      });
    }
  }

  if (current.ramAvailBytes > 0 && current.commitPressurePct > 85.0
      && current.pageFaultsDelta < 500 && current.cpuPct < 50.0) {
    findings.push_back({
      L"Memory pressure recovery.",
      L"Available memory increasing with reduced fault rate.",
      (L"Memory pressure recovery: avail " + std::to_wstring(current.ramAvailBytes / (1024*1024))
        + L" MB, faults normalized"),
      -4
    });
  }
}

} // namespace monix
