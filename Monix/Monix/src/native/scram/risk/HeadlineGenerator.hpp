#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "../../telemetry/Snapshot.hpp"
#include "SeverityTracker.hpp"

namespace monix::scram {

struct HeadlineInput {
  SeverityLevel severity = SeverityLevel::Info;
  int riskScore = 0;
  int riskDelta = 0;
  double smoothedRisk = 0.0;
  const char* topRule = nullptr;
  std::vector<std::string> diagnostics;
};

struct HeadlineOutput {
  std::wstring headline;
  std::wstring insight;
  std::string detail;
  SeverityLevel displaySeverity = SeverityLevel::Info;
};

class HeadlineGenerator {
public:
  HeadlineGenerator() = default;

  HeadlineOutput Generate(
    const HeadlineInput& input,
    const Snapshot& current,
    const Snapshot* previous)
  {
    HeadlineOutput out;
    out.displaySeverity = input.severity;

    std::wstring gpuStr;
    if (current.gpuPctValid == 1) {
      gpuStr = L" gpu=" + std::to_wstring(static_cast<int>(current.gpuPct)) + L"%";
    } else {
      gpuStr = L" gpu=unavail";
    }

    std::wstring ramStr;
    if (current.ramTotalBytes > 0) {
      const int ramUsedMB = static_cast<int>(current.ramUsedBytes / 1048576);
      const int ramTotalMB = static_cast<int>(current.ramTotalBytes / 1048576);
      const double ramPctVal = (static_cast<double>(current.ramUsedBytes) /
        static_cast<double>(current.ramTotalBytes)) * 100.0;
      ramStr = L" ram=" + std::to_wstring(ramUsedMB) + L"/" +
        std::to_wstring(ramTotalMB) + L"MB (" +
        std::to_wstring(static_cast<int>(ramPctVal + 0.5)) + L"%)";
    } else {
      ramStr = L" ram=unavail";
    }

    std::wstring topProc;
    if (!current.processes.empty()) {
      const ProcessInfo* topPtr = nullptr;
      for (const auto& p : current.processes) {
        if (p.pid <= 0) continue;
        if (topPtr == nullptr || p.cpuPct > topPtr->cpuPct) {
          topPtr = &p;
        }
      }
      if (topPtr && topPtr->cpuPct >= 5.0) {
        const bool dominant = topPtr->cpuPct > (current.cpuPct * 0.4);
        topProc = L" top=" + topPtr->name + L"(PID " + std::to_wstring(topPtr->pid) + L")"
          + L" cpu=" + std::to_wstring(static_cast<int>(topPtr->cpuPct)) + L"%";
        if (dominant) topProc += L" [dominant]";
      } else {
        topProc = L" top=diffuse";
      }
    } else {
      topProc = L" top=unavailable";
    }

    const int displayRisk = static_cast<int>(input.smoothedRisk + 0.5);
    const int deltaInt = input.riskDelta;

    std::wstring msg;
    if (input.topRule) {
      msg = std::wstring(input.topRule, input.topRule + strlen(input.topRule));
    }
    msg += L" [risk=" + std::to_wstring(displayRisk)
      + L" riskDelta=" + std::to_wstring(deltaInt) + L"]";
    msg += L" [cpu=" + std::to_wstring(static_cast<int>(current.cpuPct)) + L"%";
    msg += ramStr;
    msg += gpuStr + topProc + L"]";

    out.headline = msg;
    out.insight = L"Automated risk assessment from telemetry analysis.";

    for (const auto& d : input.diagnostics) {
      out.detail += d + "\n";
    }

    return out;
  }

  std::wstring BuildLatencyObservation(const Snapshot& s) {
    std::wstring msg;
    if (s.latencyMs >= 0) {
      msg = L"Latency " + std::to_wstring(s.latencyMs) + L"ms";
      if (s.latencySource == L"icmp") {
        msg += L" (icmp)";
      } else if (s.latencySource == L"estimated") {
        msg += L" (est)";
      }
    } else {
      msg = L"Latency unavailable";
    }
    msg += L", " + std::to_wstring(s.outboundConnections) + L" established / " +
      std::to_wstring(s.inboundConnections) + L" listening sockets.";
    return msg;
  }

  std::wstring BuildKernelObservation(const Snapshot& s) {
    const int queueLen = s.processorQueueLength;
    const int switches = s.contextSwitchesPerSec;
    const int irq = s.interruptsPerSec;
    const int handles = s.handleCount;
    const int threads = s.threadCount;

    std::wstring queueStr = queueLen < 0 ? L"unavail" :
      (queueLen == 0 ? L"0 (idle)" : std::to_wstring(queueLen));
    std::wstring switchStr = switches < 0 ? L"unavail" :
      (switches == 0 ? L"0 (stable)" : std::to_wstring(switches) + L"/s");
    std::wstring irqStr = irq < 0 ? L"unavail" :
      (irq == 0 ? L"0 (stable)" : std::to_wstring(irq) + L"/s");
    std::wstring handleStr = handles <= 0 ? L"unavail" : std::to_wstring(handles);
    std::wstring threadStr = threads <= 0 ? L"unavail" : std::to_wstring(threads);

    return L"Queue " + queueStr +
      L" | switches " + switchStr +
      L" | IRQ " + irqStr +
      L" | threads " + threadStr +
      L" | handles " + handleStr;
  }
};

}