#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "../core/Types.hpp"
#include "../logging/LogEntry.hpp"
#include "../telemetry/Snapshot.hpp"
#include "AppState.hpp"
#include "shaders/ShaderBrowserPanel.hpp"

namespace monix {

// Log view state: entries, counters, scroll, filter, history sparklines.
struct LogState {
  std::vector<LogEntry> entries;
  SessionCounters counters;
  int scroll = 0;
  LogFilter activeFilter = LogFilter::All;
  bool livePaused = false;
  std::uint64_t nextEventId = 1;
  std::vector<double> warnHistory;
  std::vector<double> errHistory;
  std::vector<double> critHistory;
  std::vector<double> kernelHistory;
  std::vector<double> netHistory;
};

// SCRAM analysis state: headline, risk score, severity tracking.
struct ScramState {
  std::wstring headline = L"SCRAM calibrating baseline\u2026";
  std::wstring insight = L"Waiting for telemetry samples before evaluating risk.";
  std::vector<std::wstring> diagnostics;
  int riskScore = 0;
  double prevRisk = 0.0;
  double smoothedRisk = 0.0;
  int severityHold = 0;
  int currentSeverity = 0;
  int errorHold = 0;
  int prevLoggedSeverity = 0;
  std::uint64_t lastScramLogNs = 0;
  int trackedPid = 0;
  int previousScramPhase = 0;
  bool overtempActive = false;
  bool critTempActive = false;
  bool gpuHotActive = false;
};

// Hardware/network history sparkline buffers.
struct HistoryBuffers {
  std::vector<double> cpu;
  std::vector<double> ram;
  std::vector<double> gpu;
  std::vector<double> netUpload;
  std::vector<double> net;
  std::vector<double> latency;
};

// Transient notification overlays.
struct NotificationState {
  std::vector<NotificationItem> items;
  std::wstring toastMessage;
  ULONGLONG toastUntilMs = 0;
};

// Shader browser / settings UI state.
struct ShaderBrowserUiState {
  renderer_vk::ShaderBrowserPanelState panel;
  std::wstring searchText;
  bool searchFocused = false;
  bool showFavoritesOnly = false;
  int selectedIndex = 0;
  int currentIndex = 0;
};

// Process appear/disappear tracking (used only by ConsumeSnapshot).
struct ProcessIdentity {
  std::wstring name;
  std::uint64_t createTime100ns = 0;
  bool operator<(const ProcessIdentity& o) const {
    if (name != o.name) return name < o.name;
    return createTime100ns < o.createTime100ns;
  }
};

struct ProcessLifeRecord {
  int seenCount = 0;
  int goneCount = 0;
  std::uint64_t lastEventSample = 0;
};

struct ProcessLifecycleState {
  std::map<int, ProcessIdentity> known;
  std::map<int, ProcessLifeRecord> records;
  bool initialized = false;
};

// Network flow burst detection and ping jitter (used only by ConsumeSnapshot).
struct NetworkFlowState {
  std::map<std::wstring, int> flowCounts;
  std::map<std::wstring, ULONGLONG> flowFirstDeltaMs;
  std::map<std::wstring, int> burstStart;
  std::map<std::wstring, int> burstEnd;
  bool csSpikeActive = false;
  std::uint64_t csSpikeFirstSample = 0;
  int csSpikePeak = 0;
  bool dpcSpikeActive = false;
  std::uint64_t dpcSpikeFirstSample = 0;
  int dpcSpikePeak = 0;
};

struct LatencyState {
  int lastEmittedLatencyMs = -1;
  int lastEmittedConnections = -1;
  bool latencyAvailable = false;
};

struct ReadyQueueState {
  bool elevated = false;
  std::uint64_t firstElevatedSample = 0;
  int peakDepth = 0;
  int occurrenceCount = 0;
};

struct DirtyBitState {
  bool tracked = false;
  bool lastValue = false;
};

// Click debug overlay state.
struct ClickDebugState {
  bool mode = false;
  POINT wnd { -1, -1 };
  POINT bmp { -1, -1 };
  ULONGLONG ms = 0;
};

// Session bookkeeping.
struct SessionInfo {
  std::wstring id;
  bool initialized = false;
  int sampleCount = 0;
};

// Windowed aggregation: accumulate metrics over N samples, emit once per window.
struct MetricsWindow {
  std::uint64_t windowStartNs = 0;
  int sampleCount = 0;

  double cpuSum = 0.0;
  double cpuMin = 100.0;
  double cpuMax = 0.0;
  double ramPctSum = 0.0;
  double ramPctMin = 100.0;
  double ramPctMax = 0.0;
  double netUpSum = 0.0;
  double netDownSum = 0.0;
  double diskReadSum = 0.0;
  double diskWriteSum = 0.0;
  double riskSum = 0.0;
  int contextSwitchSum = 0;
  int processCountSum = 0;
  int gpuSamples = 0;
  double gpuPctSum = 0.0;

  int processesCreated = 0;
  int processesTerminated = 0;

  bool IsWindowComplete(std::uint64_t nowNs, std::uint64_t windowNs) const {
    return sampleCount > 0 && (nowNs - windowStartNs) >= windowNs;
  }

  void Accumulate(const Snapshot& s, double ramPct, double risk) {
    cpuSum += s.cpuPct;
    if (s.cpuPct < cpuMin) cpuMin = s.cpuPct;
    if (s.cpuPct > cpuMax) cpuMax = s.cpuPct;
    ramPctSum += ramPct;
    if (ramPct < ramPctMin) ramPctMin = ramPct;
    if (ramPct > ramPctMax) ramPctMax = ramPct;
    netUpSum += static_cast<double>(s.netUpBytesPerSec);
    netDownSum += static_cast<double>(s.netDownBytesPerSec);
    diskReadSum += static_cast<double>(s.diskReadBytesPerSec);
    diskWriteSum += static_cast<double>(s.diskWriteBytesPerSec);
    contextSwitchSum += s.contextSwitchesPerSec;
    processCountSum += s.processCount;
    riskSum += risk;
    if (s.gpuPctValid == 1) { gpuPctSum += s.gpuPct; gpuSamples++; }
    sampleCount++;
  }

  void Reset(std::uint64_t nowNs) {
    windowStartNs = nowNs;
    sampleCount = 0;
    cpuSum = 0.0; cpuMin = 100.0; cpuMax = 0.0;
    ramPctSum = 0.0; ramPctMin = 100.0; ramPctMax = 0.0;
    netUpSum = 0.0; netDownSum = 0.0;
    diskReadSum = 0.0; diskWriteSum = 0.0;
    riskSum = 0.0;
    contextSwitchSum = 0; processCountSum = 0;
    gpuSamples = 0; gpuPctSum = 0.0;
    processesCreated = 0; processesTerminated = 0;
  }
};

// Incident correlation: groups events under a shared ID during elevated risk.
struct IncidentTracker {
  std::uint64_t nextIncidentId = 1;
  std::wstring activeCorrelationId;
  int activeSeverity = 0;
  std::uint64_t incidentStartNs = 0;

  std::wstring BeginIncident(std::uint64_t nowNs, int severity) {
    activeCorrelationId = L"INC-" + std::to_wstring(nextIncidentId++);
    activeSeverity = severity;
    incidentStartNs = nowNs;
    return activeCorrelationId;
  }

  void EndIncident() {
    activeCorrelationId.clear();
    activeSeverity = 0;
    incidentStartNs = 0;
  }

  std::wstring CurrentId() const { return activeCorrelationId; }
  bool IsActive() const { return !activeCorrelationId.empty(); }
};

// Auto-update dialog state.
struct UpdateState {
  bool dialogVisible = false;
  bool updateAvailable = false;
  bool checking = false;
  bool checkedThisSession = false;
  std::wstring latestVersion;
  std::wstring currentVersion;
  std::wstring releaseNotes;
  std::wstring downloadUrl;
  int hoverButton = -1; // 0=OK, 1=Cancel
};

} // namespace monix
