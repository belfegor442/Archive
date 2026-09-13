#include "ThreadSchedulingRule.hpp"

#include "../../telemetry/Snapshot.hpp"

namespace monix {

void ThreadSchedulingRule::Evaluate(const Snapshot& current,
                                    const Snapshot* previous,
                                    std::vector<ScramFinding>& findings) {
  if (current.processorQueueLength < 0) return;
  if (current.cpuLogicalCpus > 0 && current.processorQueueLength > static_cast<int>(current.cpuLogicalCpus) * 64) return;
  if (current.contextSwitchesPerSec < 0) return;
  if (current.interruptsPerSec < 0) return;
  if (current.processorQueueLength >= 4 && current.cpuPct < 40.0) {
    findings.push_back({
      L"CPU starvation event detected.",
      L"High ready queue with low CPU utilization suggests runnable threads are not being scheduled.",
      (L"Starvation pattern: queue " + std::to_wstring(current.processorQueueLength) + L" at " + std::to_wstring((int)current.cpuPct) + L"% CPU"),
      18
    });
  }

  if (current.readyThreadCount > 500 && current.cpuPct < 30.0 && current.processorQueueLength >= 3) {
    findings.push_back({
      L"Priority inversion pattern detected.",
      L"Many ready threads with low CPU utilization and deep queue suggests priority inversion.",
      (L"Inversion: " + std::to_wstring(current.readyThreadCount) + L" ready, " + std::to_wstring((int)current.cpuPct) + L"% CPU"),
      16
    });
  }

  if (current.isrTimePerSec > 80000000) {
    findings.push_back({
      L"High ISR latency detected.",
      L"ISR activity is extremely elevated, likely causing scheduling delays.",
      (L"ISR time: " + std::to_wstring(current.isrTimePerSec / 10000) + L"ms/s"),
      16
    });
  }

  if (current.processorQueueLength >= 6) {
    findings.push_back({
      L"Ready queue buildup detected.",
      L"Processor ready queue depth exceeds safe thresholds.",
      (L"Queue depth: " + std::to_wstring(current.processorQueueLength)),
      14
    });
  }

  if (previous) {
    const int realtimeDelta = current.realtimeThreadCount - previous->realtimeThreadCount;
    if (realtimeDelta > 20) {
      findings.push_back({
        L"RT thread count change detected.",
        L"Abnormal number of real-time priority threads detected.",
        (L"RT threads: " + std::to_wstring(previous->realtimeThreadCount) + L" -> " + std::to_wstring(current.realtimeThreadCount)),
        14
      });
    }
  }

  if (current.processorQueueLength >= 2 && current.contextSwitchesPerSec > 20000) {
    findings.push_back({
      L"Lock contention spike detected.",
      L"High context switch rate with elevated queue depth indicates lock contention.",
      (L"Lock contention spike: queue " + std::to_wstring(current.processorQueueLength) + L" with " + std::to_wstring(current.contextSwitchesPerSec) + L" context switches."),
      14
    });
  }

  if (current.contextSwitchesPerSec > 50000) {
    findings.push_back({
      L"Context switch spike detected.",
      L"Context switch rate exceeds the nominal baseline.",
      (L"Context switches: " + std::to_wstring(current.contextSwitchesPerSec) + L"/s"),
      12
    });
  }

  if (current.dpcTimePerSec > 500000) {
    findings.push_back({
      L"High DPC latency detected.",
      L"DPC execution time is elevated, potentially causing scheduling delays.",
      (L"DPC time: " + std::to_wstring(current.dpcTimePerSec / 10000) + L"ms/s"),
      12
    });
  }

  if (current.contextSwitchesPerSec > 30000 && current.cpuPct >= 85.0) {
    findings.push_back({
      L"Scheduling latency spike detected.",
      L"High context switch rate under heavy CPU load increases scheduling latency.",
      (L"Scheduling latency: " + std::to_wstring(current.contextSwitchesPerSec) + L"/s at " + std::to_wstring((int)current.cpuPct) + L"%"),
      12
    });
  }

  if (previous) {
    const int threadDelta = current.threadCount - previous->threadCount;
    if (threadDelta > 200) {
      findings.push_back({
        L"Core parking event suspected.",
        L"Thread count surge may indicate core unparking behavior.",
        (L"Thread surge: +" + std::to_wstring(threadDelta) + L" threads."),
        8
      });
    } else if (threadDelta > 100) {
      findings.push_back({
        (L"Thread creation burst: +" + std::to_wstring(threadDelta) + L" threads."),
        L"Rapid thread creation may indicate resource pressure.",
        (L"Thread creation burst: +" + std::to_wstring(threadDelta) + L" threads."),
        8
      });
    } else if (threadDelta < -100) {
      findings.push_back({
        (L"Thread count drop: " + std::to_wstring(-threadDelta) + L" threads."),
        L"Rapid thread count decrease detected.",
        (L"Thread count drop: " + std::to_wstring(-threadDelta) + L" threads."),
        6
      });
    }
  }

  if (current.threadCreationDelta > 50) {
    findings.push_back({
      (L"Thread creation spike: +" + std::to_wstring(current.threadCreationDelta) + L" this sample."),
      L"Elevated thread creation rate in current sample.",
      (L"Thread creation spike: +" + std::to_wstring(current.threadCreationDelta) + L" this sample."),
      6
    });
  }

  if (previous) {
    if (current.suspendedThreadCount > previous->suspendedThreadCount + 50) {
      const int suspendDelta = current.suspendedThreadCount - previous->suspendedThreadCount;
      findings.push_back({
        (L"Thread suspension burst: +" + std::to_wstring(suspendDelta) + L" suspended threads."),
        L"Large number of threads suspended in one sample.",
        (L"Thread suspension burst: +" + std::to_wstring(suspendDelta) + L" suspended threads."),
        6
      });
    }
    if (previous->suspendedThreadCount > current.suspendedThreadCount + 50) {
      const int resumeDelta = previous->suspendedThreadCount - current.suspendedThreadCount;
      findings.push_back({
        (L"Thread resume burst: " + std::to_wstring(resumeDelta) + L" threads resumed."),
        L"Large number of threads resumed from suspension.",
        (L"Thread resume burst: " + std::to_wstring(resumeDelta) + L" threads resumed."),
        4
      });
    }
  }
}

} // namespace monix
