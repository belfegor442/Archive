#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "../telemetry/Snapshot.hpp"
#include "../telemetry/PipelineOrchestrator.hpp"
#include "../telemetry/TelemetryBridge.hpp"
#include "../events/EventBus.hpp"
#include "../events/EventGenerator.hpp"
#include "../core/snapshot/SystemSnapshot.hpp"
#include "../core/snapshot/SnapshotAdapter.hpp"
#include "../analysis/SnapshotDiffer.hpp"
#include "../core/Timeline.hpp"
#include "../core/SessionManager.hpp"
#include "../core/TimelineBridge.hpp"
#include "../core/correlation/CorrelationEngine.hpp"
#include "../core/anomaly/AnomalyDetector.hpp"
#include "../core/scram/ScramEngine2.hpp"
#include "../core/diagnostics/DiagnosticsEngine.hpp"
#include "../core/history/ProcessHistory.hpp"
#include "../core/history/NetworkHistory.hpp"
#include "../core/history/HardwareHistory.hpp"
#include "../storage/SessionPersistence.hpp"
#include "../scram/ScramEngine.hpp"

namespace monix {

struct CoreConfig {
  std::wstring sessionBasePath = L"sessions";
  uint64_t correlationWindowNs = 5000000000ULL;
  int timelineMaxEntries = 50000;
  int sessionHistoryMax = 100;
  bool autoStartSession = true;
  bool recordToTimeline = true;
};

class MonixCore {
public:
  explicit MonixCore(const CoreConfig& config = CoreConfig{})
    : config_(config)
    , timelineBridge_(eventBus_, timeline_, sessions_)
    , sessionPersistence_(config.sessionBasePath) {
    timeline_.SetMaxEntries(config.timelineMaxEntries);
    correlationEngine_.SetWindowNs(config.correlationWindowNs);
    timelineBridge_.ConnectPersistence(
      std::make_unique<SessionPersistence>(config.sessionBasePath));
  }

  void Initialize() {
    if (config_.autoStartSession) {
      sessions_.StartSession(L"Auto Session");
    }
    timelineBridge_.StartRecording();
  }

  void Shutdown() {
    timelineBridge_.StopRecording();
    if (sessions_.HasActiveSession()) {
      sessions_.StopSession();
    }
  }

  struct ProcessResult {
    telemetry::PipelineResult pipelineResult;
    ScramResult legacyScram;
    std::vector<ScramFinding2> newFindings;
    std::vector<AnomalyResult> anomalies;
    std::vector<CorrelationCluster> clusters;
    DiagnosticReport diagnostics;
    int eventCount = 0;
    int timelineEntries = 0;
  };

  ProcessResult ProcessSnapshot(telemetry::Snapshot& snap,
                                 const telemetry::Snapshot* prev,
                                 ScramEngine& legacyScram,
                                 uint64_t timestampMs) {
    std::lock_guard<std::mutex> lock(processMutex_);
    ProcessResult result;

    uint64_t tsNs = timestampMs * 1000000ULL;

    SystemSnapshot sysSnap = SnapshotAdapter::FromLegacy(snap);
    sysSnap.timestampNs = tsNs;

    if (prev) {
      SystemSnapshot prevSys = SnapshotAdapter::FromLegacy(*prev);
      prevSys.timestampNs = tsNs - 1000000000ULL;

      SnapshotDiff diff = snapshotDiffer_.Compute(prevSys, sysSnap);
      if (diff.HasChanges()) {
        auto events = eventGenerator_.FromDiff(diff, snap.snapshotId, prev->snapshotId);
        for (auto& evt : events) {
          eventBus_.Emit(std::move(evt));
        }
        result.eventCount = static_cast<int>(events.size());
      }
    }

    result.pipelineResult = pipeline_.ProcessSnapshot(
      snap, prev, legacyScram, result.legacyScram, timestampMs, snap.snapshotId);

    for (const auto& proc : sysSnap.processes.list) {
      CorrelatedSignal sig;
      sig.timestampNs = tsNs;
      sig.source = L"process";
      sig.signalType = (proc.cpuPct > 50.0) ? L"cpu_spike" : L"process_active";
      sig.value = proc.cpuPct;
      sig.detail = proc.name;
      sig.category = EventCategory::Process;
      sig.processId = proc.pid;
      sig.processName = proc.name;
      correlationEngine_.AddSignal(sig);
    }

    if (sysSnap.cpu.pct > 80.0) {
      CorrelatedSignal sig;
      sig.timestampNs = tsNs;
      sig.source = L"cpu";
      sig.signalType = L"cpu_spike";
      sig.value = sysSnap.cpu.pct;
      sig.category = EventCategory::Hardware;
      correlationEngine_.AddSignal(sig);
    }

    if (sysSnap.network.pingRttMs > 100) {
      CorrelatedSignal sig;
      sig.timestampNs = tsNs;
      sig.source = L"network";
      sig.signalType = L"network_spike";
      sig.value = static_cast<double>(sysSnap.network.pingRttMs);
      sig.category = EventCategory::Network;
      correlationEngine_.AddSignal(sig);
    }

    if (sysSnap.thermal.cpuCoreTempC > 75.0) {
      CorrelatedSignal sig;
      sig.timestampNs = tsNs;
      sig.source = L"thermal";
      sig.signalType = L"thermal_spike";
      sig.value = sysSnap.thermal.cpuCoreTempC;
      sig.category = EventCategory::Thermal;
      correlationEngine_.AddSignal(sig);
    }

    result.clusters = correlationEngine_.DetectClusters(tsNs);

    processHistory_.RecordSnapshot(sysSnap.processes, tsNs, snap.snapshotId);
    networkHistory_.RecordSnapshot(sysSnap.network, tsNs, snap.snapshotId);
    hardwareHistory_.RecordSnapshot(sysSnap);

    auto anomalies = anomalyDetector_.Detect(sysSnap, tsNs);
    result.anomalies = anomalies;
    for (const auto& a : anomalies) {
      SystemEvent evt;
      evt.timestampNs = tsNs;
      evt.category = EventCategory::Hardware;
      evt.type = a.anomalyType;
      evt.severity = a.severity;
      evt.description = a.description;
      evt.correlationId = a.correlationId;
      eventBus_.Emit(std::move(evt));
    }

    double memUsedPct = (sysSnap.memory.totalBytes > 0) ?
      (static_cast<double>(sysSnap.memory.usedBytes) / static_cast<double>(sysSnap.memory.totalBytes)) * 100.0 : 0.0;
    anomalyDetector_.RecordBaseline(L"cpu.pct", sysSnap.cpu.pct);
    anomalyDetector_.RecordBaseline(L"memory.usedPct", memUsedPct);
    anomalyDetector_.RecordBaseline(L"gpu.pct", sysSnap.gpu.pct);
    anomalyDetector_.RecordBaseline(L"thermal.cpuCoreTempC", sysSnap.thermal.cpuCoreTempC);
    anomalyDetector_.RecordBaseline(L"network.latencyMs", static_cast<double>(sysSnap.network.latencyMs));
    anomalyDetector_.RecordBaseline(L"storage.readBytesPerSec", static_cast<double>(sysSnap.storage.readBytesPerSec));
    anomalyDetector_.RecordBaseline(L"storage.writeBytesPerSec", static_cast<double>(sysSnap.storage.writeBytesPerSec));
    anomalyDetector_.RecordBaseline(L"processes.count", static_cast<double>(sysSnap.processes.count));
    anomalyDetector_.RecordSnapshot(sysSnap);

    CorrelationCluster* clusterPtr = result.clusters.empty() ? nullptr : &result.clusters[0];
    result.newFindings = scramEngine2_.Evaluate(sysSnap, prev ? &sysSnap : nullptr,
                                                 clusterPtr, tsNs);
    for (const auto& f : result.newFindings) {
      ScramResult legacy;
      legacy.headline = f.headline;
      legacy.insight = f.insight;
      legacy.riskScore = f.riskScore;
      pipeline_.RecordScramEvent(legacy, timestampMs);
    }

    telemetryBridge_.OnSnapshotCollected(snap, tsNs);

    result.diagnostics = diagnosticsEngine_.Run(
      sysSnap, correlationEngine_, anomalyDetector_, tsNs);

    timelineBridge_.OnSnapshot(tsNs, snap.snapshotId);

    for (const auto& f : result.newFindings) {
      timelineBridge_.OnFinding(f.id, f.headline, f.riskScore, tsNs);
    }

    result.timelineEntries = timeline_.TotalEntries();

    pipeline_.BroadcastStatus(GenerateStatusJson(result));

    return result;
  }

  void StartSession(const std::wstring& name = L"") {
    sessions_.StartSession(name);
    timelineBridge_.StartRecording();
  }

  void StopSession() {
    timelineBridge_.StopRecording();
    sessions_.StopSession();
  }

  void PauseSession() { sessions_.PauseSession(); }
  void ResumeSession() { sessions_.ResumeSession(); }

  void ExportSession(const Session& session, const std::wstring& path) {
    timelineBridge_.ExportSession(session, path);
  }

  std::vector<SessionMetadata> ListSessions() {
    return timelineBridge_.ListSessions();
  }

  EventBus& GetEventBus() { return eventBus_; }
  Timeline& GetTimeline() { return timeline_; }
  SessionManager& GetSessionManager() { return sessions_; }
  CorrelationEngine& GetCorrelationEngine() { return correlationEngine_; }
  AnomalyDetector& GetAnomalyDetector() { return anomalyDetector_; }
  ScramEngine2& GetScramEngine2() { return scramEngine2_; }
  DiagnosticsEngine& GetDiagnosticsEngine() { return diagnosticsEngine_; }
  ProcessHistory& GetProcessHistory() { return processHistory_; }
  NetworkHistory& GetNetworkHistory() { return networkHistory_; }
  HardwareHistory& GetHardwareHistory() { return hardwareHistory_; }
  telemetry::PipelineOrchestrator& GetPipeline() { return pipeline_; }

  const EventBus& GetEventBus() const { return eventBus_; }
  const Timeline& GetTimeline() const { return timeline_; }
  const SessionManager& GetSessionManager() const { return sessions_; }
  const CorrelationEngine& GetCorrelationEngine() const { return correlationEngine_; }
  const AnomalyDetector& GetAnomalyDetector() const { return anomalyDetector_; }
  const ProcessHistory& GetProcessHistory() const { return processHistory_; }
  const NetworkHistory& GetNetworkHistory() const { return networkHistory_; }
  const HardwareHistory& GetHardwareHistory() const { return hardwareHistory_; }

private:
  std::wstring GenerateStatusJson(const ProcessResult& r) {
    return L"{\"score\":" + std::to_wstring(r.diagnostics.overallScore) +
      L",\"findings\":" + std::to_wstring(static_cast<int>(r.newFindings.size())) +
      L",\"anomalies\":" + std::to_wstring(static_cast<int>(r.anomalies.size())) +
      L",\"clusters\":" + std::to_wstring(static_cast<int>(r.clusters.size())) +
      L",\"events\":" + std::to_wstring(r.eventCount) +
      L",\"timeline\":" + std::to_wstring(r.timelineEntries) + L"}";
  }

  CoreConfig config_;
  EventBus eventBus_;
  Timeline timeline_;
  SessionManager sessions_;
  TimelineBridge timelineBridge_;
  SessionPersistence sessionPersistence_;
  CorrelationEngine correlationEngine_;
  AnomalyDetector anomalyDetector_;
  ScramEngine2 scramEngine2_;
  DiagnosticsEngine diagnosticsEngine_;
  ProcessHistory processHistory_;
  NetworkHistory networkHistory_;
  HardwareHistory hardwareHistory_;
  SnapshotDiffer snapshotDiffer_;
  EventGenerator eventGenerator_;
  telemetry::PipelineOrchestrator pipeline_;
  telemetry::TelemetryBridge telemetryBridge_{eventBus_};
  std::mutex processMutex_;
};

}
