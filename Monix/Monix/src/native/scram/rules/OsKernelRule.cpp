#include "OsKernelRule.hpp"

#include "../../telemetry/Snapshot.hpp"
#include "../../core/TextUtils.hpp"

namespace monix {

void OsKernelRule::Evaluate(const Snapshot& current,
                            const Snapshot* previous,
                            std::vector<ScramFinding>& findings) {
  if (!previous) return;
  const auto& prev = *previous;

  // 1. Kernel boot phase transition
  if (prev.bootPhase != current.bootPhase && prev.bootPhase >= 0 && prev.bootPhase < 3 && current.bootPhase >= 0 && current.bootPhase < 3) {
    const wchar_t* phases[] = { L"early boot", L"driver init", L"running" };
    findings.push_back({
      L"Kernel boot phase transition: " + std::wstring(phases[current.bootPhase]) + L".",
      L"System boot phase has advanced.",
      L"Boot phase: " + std::wstring(phases[prev.bootPhase]) + L" -> " + std::wstring(phases[current.bootPhase]),
      8
    });
  }

  // 2. Driver load event
  for (const auto& drv : current.driverNames) {
    if (prev.driverNames.find(drv) == prev.driverNames.end()) {
      findings.push_back({
        L"Driver load event: " + drv,
        L"A new kernel driver has been loaded into memory.",
        L"Driver loaded: " + drv,
        10
      });
    }
  }

  // 3. Driver unload event
  for (const auto& drv : prev.driverNames) {
    if (current.driverNames.find(drv) == current.driverNames.end()) {
      findings.push_back({
        L"Driver unload event: " + drv,
        L"A kernel driver has been unloaded from memory.",
        L"Driver unloaded: " + drv,
        6
      });
    }
  }

  // 4. Driver count surge (proxy for callback registration)
  if (current.driverCount > prev.driverCount + 3) {
    findings.push_back({
      L"Driver count surge detected.",
      L"Multiple drivers loaded in short interval, possibly kernel callback registration burst.",
      L"Drivers: " + std::to_wstring(prev.driverCount) + L" -> " + std::to_wstring(current.driverCount),
      12
    });
  }

  // 5. Driver count collapse
  if (prev.driverCount > current.driverCount + 5 && prev.driverCount > 0) {
    findings.push_back({
      L"Driver unload burst detected.",
      L"Multiple drivers unloaded simultaneously, possibly kernel callback failure.",
      L"Driver collapse: " + std::to_wstring(prev.driverCount) + L" -> " + std::to_wstring(current.driverCount),
      14
    });
  }

  // 6. Handle object count anomaly (proxy for kernel object creation/deletion)
  if (prev.totalObjects > 0 && current.totalObjects > 0) {
    const long long objDelta = static_cast<long long>(current.totalObjects) - static_cast<long long>(prev.totalObjects);
    if (objDelta > 50000) {
      findings.push_back({
        L"Kernel object creation surge.",
        L"Object count jumped by over 50k in one sample, indicating mass kernel object allocation.",
        L"Objects: +" + std::to_wstring(objDelta),
        14
      });
    } else if (objDelta < -50000) {
      findings.push_back({
        L"Kernel object mass deletion.",
        L"Object count dropped by over 50k, indicating mass kernel object release.",
        L"Objects: " + std::to_wstring(objDelta),
        10
      });
    }
  }

  // 7. Handle count anomaly
  if (prev.totalHandles > 0 && current.totalHandles > 0) {
    const long long hDelta = static_cast<long long>(current.totalHandles) - static_cast<long long>(prev.totalHandles);
    if (hDelta > 30000) {
      findings.push_back({
        L"Handle table surge detected.",
        L"Handle count jumped by over 30k, possibly indicating a handle leak.",
        L"Handles: +" + std::to_wstring(hDelta),
        12
      });
    } else if (hDelta < -30000) {
      findings.push_back({
        L"Handle mass release detected.",
        L"Handle count dropped by over 30k.",
        L"Handle mass release: " + std::to_wstring(hDelta),
        4
      });
    }
  }

  // 8. Time change detection
  if (prev.systemTime100ns > 0 && current.systemTime100ns > 0) {
    const long long timeDelta = static_cast<long long>(current.systemTime100ns - prev.systemTime100ns);
    const long long expectedDelta = (current.uptimeMs - prev.uptimeMs) * 10000LL;
    const long long drift = timeDelta - expectedDelta;
    if (drift > 100000000LL || drift < -100000000LL) {
      findings.push_back({
        L"System time change detected.",
        L"System clock has been adjusted by more than 10 seconds.",
        L"Time drift: " + std::to_wstring(drift / 10000) + L" ms",
        16
      });
    }
  }

  // 9. Session start event
  if (current.sessionCount > prev.sessionCount && prev.sessionCount > 0) {
    findings.push_back({
      L"Session start event detected.",
      L"New user session has been created on the system.",
      L"Sessions: " + std::to_wstring(prev.sessionCount) + L" -> " + std::to_wstring(current.sessionCount),
      8
    });
  }

  // 10. Session end event
  if (prev.sessionCount > current.sessionCount && current.sessionCount > 0) {
    findings.push_back({
      L"Session end event detected.",
      L"A user session has been terminated.",
      L"Sessions: " + std::to_wstring(prev.sessionCount) + L" -> " + std::to_wstring(current.sessionCount),
      6
    });
  }

  // 11. Critical process termination (session count drop to 0)
  if (prev.sessionCount > 0 && current.sessionCount == 0) {
    findings.push_back({
      L"Critical session collapse detected.",
      L"All user sessions have been terminated, indicating system-level process termination.",
      L"Session collapse: " + std::to_wstring(prev.sessionCount) + L" -> 0",
      20
    });
  }

  // 12. Process count burst (proxy for process notify events)
  const int procDelta = current.processCount - prev.processCount;
  if (procDelta > 50) {
    findings.push_back({
      L"Process creation burst detected.",
      L"Process count increased by over 50 in one sample, indicating mass process creation.",
      L"Processes: +" + std::to_wstring(procDelta),
      10
    });
  } else if (procDelta < -50) {
    findings.push_back({
      L"Process termination burst detected.",
      L"Process count decreased by over 50 in one sample, indicating mass process termination.",
      L"Processes: " + std::to_wstring(procDelta),
      8
    });
  }

  // 13. Thread notify burst
  const int threadDelta = current.threadCount - prev.threadCount;
  if (threadDelta > 200) {
    findings.push_back({
      L"Thread creation burst detected.",
      L"Thread count increased by over 200 in one sample.",
      L"Threads: +" + std::to_wstring(threadDelta),
      10
    });
  } else if (threadDelta < -200) {
    findings.push_back({
      L"Thread count drop detected.",
      L"Thread count decreased by over 200 in one sample.",
      L"Threads: " + std::to_wstring(-threadDelta),
      8
    });
  }

  // 14. Page fault spike (proxy for system call anomaly)
  if (prev.pageFaultsDelta > 0 && current.pageFaultsDelta > 0) {
    const long long pfDelta = static_cast<long long>(current.pageFaultsDelta) - static_cast<long long>(prev.pageFaultsDelta);
    if (pfDelta > 100000) {
      findings.push_back({
        L"System call anomaly detected.",
        L"Page fault count surged by over 100k, indicating unusual memory access patterns.",
        L"Page faults: +" + std::to_wstring(pfDelta),
        12
      });
    }
  }

  // 15. I/O read burst (proxy for image load notify events)
  if (prev.ioReadBytesDelta > 0 && current.ioReadBytesDelta > 0) {
    const long long ioDelta = static_cast<long long>(current.ioReadBytesDelta) - static_cast<long long>(prev.ioReadBytesDelta);
    if (ioDelta > 500 * 1024 * 1024) {
      findings.push_back({
        L"I/O read burst detected.",
        L"Read I/O increased by over 500 MB, possibly indicating large image load or driver read activity.",
        L"I/O read: +" + std::to_wstring(ioDelta / (1024 * 1024)) + L" MB",
        10
      });
    }
  }

  // 16. I/O write burst (proxy for registry callback events)
  if (prev.ioWriteBytesDelta > 0 && current.ioWriteBytesDelta > 0) {
    const long long ioDelta = static_cast<long long>(current.ioWriteBytesDelta) - static_cast<long long>(prev.ioWriteBytesDelta);
    if (ioDelta > 200 * 1024 * 1024) {
      findings.push_back({
        L"I/O write burst detected.",
        L"Write I/O increased by over 200 MB, possibly indicating registry or config write activity.",
        L"I/O write: +" + std::to_wstring(ioDelta / (1024 * 1024)) + L" MB",
        10
      });
    }
  }

  // 17. Interrupt request event spike (use normalized ISR time rate)
  if (current.isrTimePerSec > 500000000) {
    findings.push_back({
      L"Interrupt request event spike.",
      L"ISR execution time exceeded 50ms/s, indicating elevated interrupt activity.",
      (L"ISR time: " + std::to_wstring(current.isrTimePerSec / 10000) + L"ms/s"),
      14
    });
  }

  // 18. DPC event spike (dpcTimePerSec is cumulative DPC execution time delta per second in 100ns units)
  // 300000 = 30ms of DPC time per second — sustained high DPC load
  if (current.dpcTimePerSec > 300000) {
    findings.push_back({
      L"Deferred procedure call spike.",
      L"DPC execution time exceeded 30ms/s, indicating sustained high deferred procedure activity.",
      (L"DPC time: " + std::to_wstring(current.dpcTimePerSec / 10000) + L"ms/s"),
      14
    });
  } else if (current.dpcTimePerSec > 100000) {
    findings.push_back({
      L"Elevated deferred procedure call activity.",
      L"DPC execution time above normal baseline.",
      (L"DPC time: " + std::to_wstring(current.dpcTimePerSec / 10000) + L"ms/s"),
      6
    });
  }

  // 19. Context switch anomaly
  if (current.contextSwitchesPerSec >= 0) {
    const uint64_t csDelta = current.totalContextSwitches >= prev.totalContextSwitches ? current.totalContextSwitches - prev.totalContextSwitches : 0;
    if (csDelta > 200000) {
      findings.push_back({
        L"Context switch anomaly detected.",
        L"Context switch count surged by over 200k, indicating extreme scheduling pressure.",
        L"CS: +" + std::to_wstring(csDelta),
        16
      });
    }
  }

  // 20. Watchdog timeout precursor (high CPU + high CS + high ISR)
  if (current.cpuPct >= 95.0 && current.contextSwitchesPerSec > 40000 && current.isrTimePerSec > 200000000) {
    findings.push_back({
      L"Watchdog timeout precursor detected.",
      L"System is under extreme load with high CPU, context switches, and interrupts \xe2\x80\x94 watchdog may trigger.",
      L"Watchdog: CPU " + std::to_wstring((int)current.cpuPct) + L"%, CS " + std::to_wstring(current.contextSwitchesPerSec) + L"/s",
      20
    });
  }

  // 21. Kernel panic precursor (multiple high-risk indicators)
  if (current.cpuPct >= 98.0 && current.ramTotalBytes > 0 && current.ramUsedBytes > current.ramTotalBytes * 95 / 100 && current.diskQueueLength >= 4.0) {
    findings.push_back({
      L"Kernel panic precursor detected.",
      L"System is critically overloaded: max CPU, exhausted RAM, and deep disk queue.",
      L"Panic precursor: CPU " + std::to_wstring((int)current.cpuPct) + L"%, RAM " + FormatPercent(current.ramUsedBytes * 100.0 / current.ramTotalBytes) + L", DQ " + std::to_wstring(current.diskQueueLength),
      20
    });
  }

  // 22. Bugcheck precursor (high DPC + high ISR + long scheduling delay)
  if (current.readyThreadCount > 500 && current.cpuPct >= 90.0 && current.contextSwitchesPerSec > 50000) {
    findings.push_back({
      L"Bugcheck precursor detected.",
      L"High ready thread count with extreme CPU and context switches may precede a bugcheck.",
      L"Bugcheck risk: " + std::to_wstring(current.readyThreadCount) + L" ready, " + std::to_wstring((int)current.cpuPct) + L"% CPU",
      18
    });
  }

  // 23. Kernel object creation (handle table growth)
  if (prev.totalHandles > 0 && current.totalHandles > 0) {
    const long long hGrowth = static_cast<long long>(current.totalHandles) - static_cast<long long>(prev.totalHandles);
    if (hGrowth > 10000 && hGrowth <= 30000) {
      findings.push_back({
        L"Handle table growth detected.",
        L"Handle count growing steadily, indicating kernel object accumulation.",
        L"Handle growth: +" + std::to_wstring(hGrowth) + L" (steady)",
        6
      });
    }
  }

  // 24. Device stack change (driver count delta — only significant changes to avoid flaky registry reads)
  if (current.driverCount > prev.driverCount && current.driverCount - prev.driverCount > 5) {
    findings.push_back({
      L"Device stack change detected.",
      L"Significant driver count change, possibly new device drivers loaded.",
      L"Device stack change: +" + std::to_wstring(current.driverCount - prev.driverCount) + L" driver(s)",
      6
    });
  }

  // 25. System uptime anomaly (unexpected reboot detection)
  if (current.uptimeMs < prev.uptimeMs && prev.uptimeMs > 60000) {
    findings.push_back({
      L"Unexpected system reboot detected.",
      L"System uptime has decreased, indicating an unexpected restart or crash.",
      L"Uptime: " + std::to_wstring(prev.uptimeMs / 1000) + L"s -> " + std::to_wstring(current.uptimeMs / 1000) + L"s",
      20
    });
  }
}

} // namespace monix
