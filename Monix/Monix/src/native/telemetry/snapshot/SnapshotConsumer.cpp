#include "SnapshotConsumer.hpp"

#include "../../MonixApp.hpp"
#include "../../TelemetryInternal.hpp"
#include "../../core/TextUtils.hpp"

#include <windows.h>
#include <algorithm>
#include <cmath>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "telemetry/state/ProcessChangeDetector.hpp"
#include "telemetry/state/ThresholdDetector.hpp"
#include "telemetry/state/SnapshotChangeDetector.hpp"
#include "telemetry/state/UeoCorrelator.hpp"

using namespace monix;
using namespace monix::telemetry;

static double ComputeRamPct(const Snapshot& s) {
  if (s.ramTotalBytes == 0) return 0.0;
  return (static_cast<double>(s.ramUsedBytes) / static_cast<double>(s.ramTotalBytes)) * 100.0;
}

static double ComputeRiskScore(const Snapshot& cur, const Snapshot* prev) {
  double risk = 0.0;

  // CPU pressure (0-30 points)
  risk += std::min(30.0, cur.cpuPct * 0.3);

  // RAM pressure (0-25 points)
  const double ramPct = ComputeRamPct(cur);
  risk += std::min(25.0, ramPct * 0.25);

  // GPU pressure (0-15 points)
  if (cur.gpuPctValid == 1) {
    risk += std::min(15.0, cur.gpuPct * 0.15);
  }

  // Thermal stress (0-15 points)
  if (cur.cpuCoreTempC > 0.0) {
    if (cur.cpuCoreTempC > 90.0) risk += 15.0;
    else if (cur.cpuCoreTempC > 80.0) risk += 10.0;
    else if (cur.cpuCoreTempC > 70.0) risk += 5.0;
  }
  if (cur.gpuTempC > 85.0) risk += 8.0;
  else if (cur.gpuTempC > 75.0) risk += 4.0;

  // Context switch storm (0-10 points)
  if (cur.contextSwitchesPerSec > 50000) risk += 10.0;
  else if (cur.contextSwitchesPerSec > 20000) risk += 6.0;
  else if (cur.contextSwitchesPerSec > 10000) risk += 3.0;

  // Processor queue depth (0-5 points)
  if (cur.processorQueueLength > 4) risk += 5.0;
  else if (cur.processorQueueLength > 2) risk += 2.0;

  // Delta acceleration: if CPU jumped sharply from previous sample
  if (prev) {
    const double cpuDelta = cur.cpuPct - prev->cpuPct;
    if (cpuDelta > 20.0) risk += 10.0;
    else if (cpuDelta > 10.0) risk += 5.0;

    const double ramDelta = static_cast<double>(cur.ramUsedBytes) - static_cast<double>(prev->ramUsedBytes);
    if (ramDelta > 500000000.0) risk += 8.0;  // 500MB jump
    else if (ramDelta > 200000000.0) risk += 4.0;  // 200MB jump
  }

  return std::min(100.0, std::max(0.0, risk));
}

static std::wstring BuildHeadline(double risk, int severity) {
  if (severity >= 3) return L"CRITICAL: System under extreme stress";
  if (severity >= 2) return L"ERROR: Elevated system pressure detected";
  if (severity >= 1) return L"WARNING: Resource utilization climbing";
  if (risk > 20.0) return L"Nominal: Minor resource fluctuations";
  return L"System nominal — all metrics within baseline";
}

static std::wstring BuildInsight(const Snapshot& s) {
  std::wstring insight;
  const double ramPct = ComputeRamPct(s);

  if (s.cpuPct > 80.0) {
    insight += L"CPU saturated (" + std::to_wstring(static_cast<int>(s.cpuPct)) + L"%). ";
  } else if (s.cpuPct > 50.0) {
    insight += L"CPU moderately loaded (" + std::to_wstring(static_cast<int>(s.cpuPct)) + L"%). ";
  }

  if (ramPct > 85.0) {
    insight += L"RAM critical (" + std::to_wstring(static_cast<int>(ramPct)) + L"% used). ";
  } else if (ramPct > 70.0) {
    insight += L"RAM elevated (" + std::to_wstring(static_cast<int>(ramPct)) + L"% used). ";
  }

  if (s.gpuPctValid == 1 && s.gpuPct > 90.0) {
    insight += L"GPU at " + std::to_wstring(static_cast<int>(s.gpuPct)) + L"%. ";
  }

  if (s.cpuCoreTempC > 80.0) {
    insight += L"CPU temp " + std::to_wstring(static_cast<int>(s.cpuCoreTempC)) + L"C. ";
  }

  if (s.contextSwitchesPerSec > 20000) {
    insight += L"Context switches elevated (" + std::to_wstring(s.contextSwitchesPerSec) + L"/s). ";
  }

  if (insight.empty()) {
    insight = L"All subsystems within normal operating range.";
  }
  return insight;
}

void MonixApp::ConsumeSnapshot(Snapshot snapshot) {
  const bool hadPrevious = state_.hasPreviousSnapshot;
  state_.previousSnapshot = std::make_unique<Snapshot>(std::move(state_.snapshot));
  state_.hasPreviousSnapshot = true;
  AppendHistoryPoint(snapshot);
  ++state_.session.sampleCount;
  const auto& previous = *state_.previousSnapshot;

  auto& health = logManager_->HealthManager();
  std::uint64_t monotonicNs = 0;
  {
    LARGE_INTEGER freq, now;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&now);
    monotonicNs = static_cast<std::uint64_t>(now.QuadPart) * 1000000000ULL /
      static_cast<std::uint64_t>(freq.QuadPart);
  }

  auto& cpuSrc = health.GetSource(L"cpu");
  cpuSrc.name = L"cpu";
  cpuSrc.staleAfterNs = 3000000000ULL;
  cpuSrc.RecordSuccess(monotonicNs);

  auto& ramSrc = health.GetSource(L"ram");
  ramSrc.name = L"ram";
  ramSrc.staleAfterNs = 3000000000ULL;
  ramSrc.RecordSuccess(monotonicNs);

  auto& gpuSrc = health.GetSource(L"gpu");
  gpuSrc.name = L"gpu";
  gpuSrc.staleAfterNs = 5000000000ULL;
  if (snapshot.gpuPctValid == 1) {
    gpuSrc.RecordSuccess(monotonicNs);
  } else {
    gpuSrc.RecordFailure(monotonicNs, L"gpuPctValid != 1");
  }

  auto& netSrc = health.GetSource(L"network");
  netSrc.name = L"network";
  netSrc.staleAfterNs = 10000000000ULL;
  if (snapshot.latencyMs >= 0 || snapshot.outboundConnections > 0) {
    netSrc.RecordSuccess(monotonicNs);
  } else {
    netSrc.RecordFailure(monotonicNs, L"no network data");
  }

  auto& diskSrc = health.GetSource(L"disk");
  diskSrc.name = L"disk";
  diskSrc.staleAfterNs = 5000000000ULL;
  if (snapshot.diskReadBytesPerSec > 0 || snapshot.diskWriteBytesPerSec > 0) {
    diskSrc.RecordSuccess(monotonicNs);
  } else {
    diskSrc.RecordFailure(monotonicNs, L"no disk I/O");
  }

  auto& thermalSrc = health.GetSource(L"thermal");
  thermalSrc.name = L"thermal";
  thermalSrc.staleAfterNs = 10000000000ULL;
  if (snapshot.cpuCoreTempC > 0.0 || snapshot.gpuTempC > 0.0) {
    thermalSrc.RecordSuccess(monotonicNs);
  } else {
    thermalSrc.RecordFailure(monotonicNs, L"no thermal data");
  }

  auto& processSrc = health.GetSource(L"process");
  processSrc.name = L"process";
  processSrc.staleAfterNs = 3000000000ULL;
  if (!snapshot.processes.empty()) {
    processSrc.RecordSuccess(monotonicNs);
  } else {
    processSrc.RecordFailure(monotonicNs, L"processes empty");
  }

  health.CheckStaleness(monotonicNs);
  health.ResetAllTick();

  if (!state_.session.initialized) {
    PushLog(L"SYSTEM", L"SUCCESS", L"Collector synchronized with native render surface.", ColorRole::Success);
    PushLog(L"KERNEL", L"INFO", L"Kernel counters and thermal matrix are online.", ColorRole::Kernel);
    state_.session.initialized = true;
  }

  // --- Compute risk and accumulate into window ---
  const double ramPct = ComputeRamPct(snapshot);
  const double currentRisk = ComputeRiskScore(snapshot, hadPrevious ? &previous : nullptr);
  state_.scramState.riskScore = static_cast<int>(currentRisk + 0.5);
  state_.scramState.smoothedRisk = state_.scramState.smoothedRisk * 0.7 + currentRisk * 0.3;

  auto& win = state_.metricsWindow;
  if (win.sampleCount == 0) {
    win.windowStartNs = monotonicNs;
  }
  win.Accumulate(snapshot, ramPct, currentRisk);

  // --- Process lifecycle: only detect AFTER first snapshot ---
  ChangeSet processChanges;
  state_.processDetector.Detect(snapshot, hadPrevious ? &previous : nullptr, state_.session.sampleCount, processChanges);
  if (hadPrevious) {
    for (const auto& evt : processChanges.events) {
      if (evt.kind == ChangeEvent::Kind::Created) {
        win.processesCreated++;
        std::wstring detail = L"pid=" + std::to_wstring(evt.entity.idLow);
        const ProcessInfo* found = nullptr;
        for (const auto& p : snapshot.processes) {
          if (p.pid == evt.entity.idLow) { found = &p; break; }
        }
        if (found) {
          detail += L" " + found->name
            + L" cpu=" + std::to_wstring(static_cast<int>(found->cpuPct)) + L"%"
            + L" ram=" + std::to_wstring(found->ramBytes / 1048576) + L"MB";
          if (found->cpuPct > 50.0) {
            state_.ueoCorrelator.AddSignal({L"process_created", found->name, found->cpuPct, monotonicNs});
          }
        }
        PushLog(L"PROCESS", L"INFO", detail, ColorRole::Success,
          L"process", L"lifecycle", L"event=process_created");
      } else if (evt.kind == ChangeEvent::Kind::Terminated) {
        win.processesTerminated++;
        PushLog(L"PROCESS", L"WARNING",
          L"pid=" + std::to_wstring(evt.entity.idLow) + L" exited",
          ColorRole::Warning, L"process", L"lifecycle", L"event=process_terminated");
      }
    }
  }

  // --- Thermal alerts (immediate, crossing threshold) ---
  if (snapshot.cpuCoreTempC > 85.0 && (!hadPrevious || previous.cpuCoreTempC <= 85.0)) {
    state_.ueoCorrelator.AddSignal({L"thermal", L"CPU " + std::to_wstring(static_cast<int>(snapshot.cpuCoreTempC)) + L"C", snapshot.cpuCoreTempC, monotonicNs});
    PushLog(L"THERMAL", L"WARNING",
      L"CPU temp " + std::to_wstring(static_cast<int>(snapshot.cpuCoreTempC)) + L"C",
      ColorRole::Warning, L"thermal", L"cpu", L"event=thermal_cpu_high");
  }
  if (snapshot.cpuCoreTempC > 95.0 && (!hadPrevious || previous.cpuCoreTempC <= 95.0)) {
    state_.ueoCorrelator.AddSignal({L"thermal", L"CPU CRITICAL " + std::to_wstring(static_cast<int>(snapshot.cpuCoreTempC)) + L"C", snapshot.cpuCoreTempC, monotonicNs});
    PushLog(L"THERMAL", L"CRITICAL",
      L"CPU CRITICAL " + std::to_wstring(static_cast<int>(snapshot.cpuCoreTempC)) + L"C",
      ColorRole::Fatal, L"thermal", L"cpu", L"event=thermal_cpu_critical");
  }
  if (snapshot.gpuPctValid == 1 && snapshot.gpuTempC > 85.0 && (!hadPrevious || previous.gpuTempC <= 85.0)) {
    PushLog(L"THERMAL", L"WARNING",
      L"GPU temp " + std::to_wstring(static_cast<int>(snapshot.gpuTempC)) + L"C",
      ColorRole::Warning, L"thermal", L"gpu", L"event=thermal_gpu_high");
  }

  // --- Window evaluation: every 5 seconds ---
  constexpr std::uint64_t kWindowNs = 5000000000ULL;
  if (hadPrevious && win.IsWindowComplete(monotonicNs, kWindowNs)) {
    const int n = win.sampleCount;
    const double avgCpu = win.cpuSum / n;
    const double avgRamPct = win.ramPctSum / n;
    const double avgRisk = win.riskSum / n;
    const double avgNetUp = win.netUpSum / n;
    const double avgNetDown = win.netDownSum / n;
    const double avgDiskR = win.diskReadSum / n;
    const double avgDiskW = win.diskWriteSum / n;
    const int avgCS = win.contextSwitchSum / n;
    const int avgProc = win.processCountSum / n;

    state_.scramState.smoothedRisk = state_.scramState.smoothedRisk * 0.7 + avgRisk * 0.3;
    const double effectiveRisk = state_.scramState.smoothedRisk;
    int rawSeverity = effectiveRisk >= 70 ? 3 : effectiveRisk >= 50 ? 2 : effectiveRisk >= 30 ? 1 : 0;

    if (rawSeverity == 3) {
      state_.scramState.errorHold++;
    } else {
      state_.scramState.errorHold = 0;
    }
    if (state_.scramState.errorHold >= 3 && rawSeverity >= 2) {
      rawSeverity = 3;
    }

    if (rawSeverity > state_.scramState.currentSeverity) {
      state_.scramState.currentSeverity = rawSeverity;
      state_.scramState.severityHold = 0;
    } else if (rawSeverity < state_.scramState.currentSeverity) {
      state_.scramState.severityHold++;
      if (state_.scramState.severityHold < 3) {
        rawSeverity = state_.scramState.currentSeverity;
      } else {
        state_.scramState.currentSeverity = rawSeverity;
        state_.scramState.severityHold = 0;
      }
    } else {
      state_.scramState.severityHold = 0;
    }

    // Correlation: start/end incidents
    auto& inc = state_.incidentTracker;
    if (state_.scramState.currentSeverity >= 1 && !inc.IsActive()) {
      inc.BeginIncident(monotonicNs, state_.scramState.currentSeverity);
    } else if (state_.scramState.currentSeverity == 0 && inc.IsActive()) {
      inc.EndIncident();
    }
    const std::wstring corrMeta = inc.IsActive()
      ? L"corr=" + inc.CurrentId() + L" "
      : L"";

    // SCRAM report on severity change
    const bool severityChanged = state_.scramState.currentSeverity != state_.scramState.prevLoggedSeverity;
    const bool cooldownExpired = (monotonicNs - state_.scramState.lastScramLogNs) >= 30000000000ULL;
    if (severityChanged && cooldownExpired) {
      state_.scramState.prevLoggedSeverity = state_.scramState.currentSeverity;
      state_.scramState.lastScramLogNs = monotonicNs;
      const ColorRole color = state_.scramState.currentSeverity == 3 ? ColorRole::CriticalScram
                        : state_.scramState.currentSeverity == 2 ? ColorRole::Error
                        : state_.scramState.currentSeverity == 1 ? ColorRole::Warning
                        : ColorRole::Scram;
      const std::wstring sevText = state_.scramState.currentSeverity == 3 ? L"CRITICAL"
                         : state_.scramState.currentSeverity == 2 ? L"ERROR"
                         : state_.scramState.currentSeverity == 1 ? L"WARNING"
                         : L"INFO";

      std::wstring topProc;
      if (!snapshot.processes.empty()) {
        const ProcessInfo* topPtr = nullptr;
        for (const auto& p : snapshot.processes) {
          if (p.pid <= 0) continue;
          if (topPtr == nullptr || p.cpuPct > topPtr->cpuPct) topPtr = &p;
        }
        if (topPtr && topPtr->cpuPct >= 10.0) {
          topProc = L" " + topPtr->name + L" cpu=" + std::to_wstring(static_cast<int>(topPtr->cpuPct)) + L"%";
        }
      }

      std::wstring msg;
      msg += L"risk=" + std::to_wstring(static_cast<int>(avgRisk + 0.5))
        + L" cpu=" + std::to_wstring(static_cast<int>(avgCpu)) + L"%"
        + L" ram=" + std::to_wstring(static_cast<int>(avgRamPct)) + L"%"
        + topProc;
      PushLog(L"SCRAM", sevText, msg, color, L"scram", L"risk",
        corrMeta + L"event=scram_risk_report",
        rawSeverity >= 2 ? EventType::Anomaly : EventType::Warning);
    }

    // Resource spike: window-over-window comparison
    if (state_.hasPreviousSnapshot) {
      const double prevCpu = previous.cpuPct;
      const double cpuDelta = avgCpu - prevCpu;
      const int procDelta = snapshot.processCount - avgProc;
      if (cpuDelta > 20.0 || procDelta > 20) {
        state_.ueoCorrelator.AddSignal({L"cpu_spike", L"delta=" + std::to_wstring(static_cast<int>(cpuDelta)), avgCpu, monotonicNs});
        std::wstring msg;
        msg += L"cpu=" + std::to_wstring(static_cast<int>(avgCpu)) + L"%"
          + L"(+" + std::to_wstring(static_cast<int>(cpuDelta)) + L")"
          + L" ram=" + std::to_wstring(static_cast<int>(avgRamPct)) + L"%"
          + L" proc=" + std::to_wstring(avgProc);
        PushLog(L"RESOURCE", L"WARNING", msg, ColorRole::Warning,
          L"resource", L"spike", corrMeta + L"event=resource_spike");
      }
    }

    // Network: only if high throughput sustained in window
    if (avgNetDown > 10485760.0) {
      state_.ueoCorrelator.AddSignal({L"network_spike", L"down=" + std::to_wstring(static_cast<int>(avgNetDown / 1048576)) + L"MB/s", avgNetDown / 1048576.0, monotonicNs});
      PushLog(L"NETWORK", L"INFO",
        L"Avg download: " + std::to_wstring(static_cast<int>(avgNetDown / 1048576)) + L"MB/s",
        ColorRole::Network, L"network", L"throughput", corrMeta + L"event=network_throughput");
    }

    // Reset window
    win.Reset(monotonicNs);
  }

  // --- UEO flush: emit correlated incidents ---
  if (state_.ueoCorrelator.ShouldFlush(monotonicNs)) {
    auto incident = state_.ueoCorrelator.Flush(monotonicNs, state_.nextUeoIncidentId++);
    if (incident.severity >= 1 && !incident.signals.empty()) {
      const ColorRole color = incident.severity >= 2 ? ColorRole::Error : ColorRole::Warning;
      const std::wstring sev = incident.severity >= 2 ? L"ERROR" : L"WARNING";
      std::wstring signalTypes;
      for (size_t i = 0; i < incident.signals.size(); ++i) {
        if (i > 0) signalTypes += L"+";
        signalTypes += incident.signals[i].type;
      }
      std::wstring msg = incident.summary
        + L" [" + signalTypes + L"]"
        + L" signals=" + std::to_wstring(static_cast<int>(incident.signals.size()));
      PushLog(L"UEO", sev, msg, color, L"ueo", L"correlate",
        L"corr=" + incident.correlationId + L" root=" + incident.rootCause + L" event=ueo_incident");
    }
  }

  state_.snapshot = std::move(snapshot);
}
