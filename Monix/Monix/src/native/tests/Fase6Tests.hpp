#pragma once

#include "../core/MonixCore.hpp"
#include "../core/CollectorBridge.hpp"
#include "../events/EventBus.hpp"
#include "../core/Timeline.hpp"
#include "../core/SessionManager.hpp"
#include "../core/correlation/CorrelationEngine.hpp"
#include "../core/anomaly/AnomalyDetector.hpp"
#include "../core/scram/ScramEngine2.hpp"
#include "../core/diagnostics/DiagnosticsEngine.hpp"
#include "../core/history/ProcessHistory.hpp"
#include "../core/history/NetworkHistory.hpp"
#include "../core/history/HardwareHistory.hpp"
#include "../telemetry/Snapshot.hpp"
#include "../scram/ScramEngine.hpp"

#include <cassert>
#include <cstdio>
#include <string>

namespace monix::tests {

inline void TestMonixCoreInit() {
  printf("  TestMonixCoreInit... ");

  CoreConfig config;
  config.autoStartSession = false;
  config.recordToTimeline = false;
  MonixCore core(config);

  assert(!core.GetEventBus().HasSubscribers());
  assert(core.GetTimeline().Empty());
  assert(!core.GetSessionManager().HasActiveSession());
  assert(core.GetCorrelationEngine().SignalCount() == 0);
  assert(core.GetProcessHistory().AliveCount() == 0);
  assert(core.GetNetworkHistory().SampleCount() == 0);

  core.Initialize();
  assert(core.GetSessionManager().HasActiveSession());

  core.Shutdown();

  printf("OK\n");
}

inline void TestMonixCoreProcess() {
  printf("  TestMonixCoreProcess... ");

  CoreConfig config;
  config.autoStartSession = true;
  config.recordToTimeline = true;
  MonixCore core(config);
  core.Initialize();

  int eventCount = 0;
  core.GetEventBus().SubscribeAll([&](const SystemEvent&) { eventCount++; });

  telemetry::Snapshot prev;
  prev.snapshotId = 1;
  prev.cpuPct = 50.0;
  prev.ramUsedBytes = 8ULL * 1024 * 1024 * 1024;
  prev.ramTotalBytes = 16ULL * 1024 * 1024 * 1024;
  prev.gpuPct = 40.0;
  prev.gpuTempC = 65.0;
  prev.cpuCoreTempC = 65.0;
  prev.rttMs = 20.0;
  prev.downKbps = 1000.0;
  prev.activeConns = 50;
  prev.droppedPktPct = 0.1;
  prev.tcpRetransmitPct = 0.5;
  prev.diskReadMBs = 100.0;
  prev.diskWriteMBs = 50.0;
  prev.processCount = 180;

  telemetry::Snapshot snap = prev;
  snap.snapshotId = 2;
  snap.cpuPct = 92.0;
  snap.cpuCoreTempC = 82.0;
  snap.rttMs = 150.0;
  snap.processCount = 200;

  ScramEngine legacyScram;
  auto result = core.ProcessSnapshot(snap, &prev, legacyScram, 2000);

  assert(result.pipelineResult.normalized);
  assert(result.pipelineResult.validated);
  assert(result.eventCount >= 0);
  assert(result.timelineEntries > 0);

  assert(core.GetTimeline().TotalEntries() > 0);
  assert(core.GetSessionManager().HasActiveSession());

  core.Shutdown();

  printf("OK\n");
}

inline void TestMonixCoreHistory() {
  printf("  TestMonixCoreHistory... ");

  CoreConfig config;
  config.autoStartSession = true;
  MonixCore core(config);
  core.Initialize();

  telemetry::Snapshot prev;
  prev.snapshotId = 1;
  prev.cpuPct = 50.0;
  prev.ramUsedBytes = 8ULL * 1024 * 1024 * 1024;
  prev.ramTotalBytes = 16ULL * 1024 * 1024 * 1024;
  prev.gpuPct = 40.0;
  prev.gpuTempC = 65.0;
  prev.cpuCoreTempC = 65.0;
  prev.rttMs = 20.0;
  prev.downKbps = 1000.0;
  prev.activeConns = 50;
  prev.droppedPktPct = 0.1;
  prev.tcpRetransmitPct = 0.5;
  prev.diskReadMBs = 100.0;
  prev.diskWriteMBs = 50.0;
  prev.processCount = 180;

  telemetry::Snapshot snap = prev;
  snap.snapshotId = 2;
  snap.cpuPct = 55.0;

  ScramEngine legacyScram;
  core.ProcessSnapshot(snap, &prev, legacyScram, 1000);

  assert(core.GetProcessHistory().AliveCount() >= 0);
  assert(core.GetNetworkHistory().SampleCount() > 0);

  snap.snapshotId = 3;
  snap.cpuPct = 60.0;
  core.ProcessSnapshot(snap, &prev, legacyScram, 2000);

  assert(core.GetTimeline().TotalEntries() > 0);

  core.Shutdown();

  printf("OK\n");
}

inline void TestMonixCoreEvents() {
  printf("  TestMonixCoreEvents... ");

  CoreConfig config;
  config.autoStartSession = true;
  MonixCore core(config);
  core.Initialize();

  int eventCount = 0;
  core.GetEventBus().SubscribeAll([&](const SystemEvent&) { eventCount++; });

  telemetry::Snapshot prev;
  prev.snapshotId = 1;
  prev.cpuPct = 50.0;
  prev.ramUsedBytes = 8ULL * 1024 * 1024 * 1024;
  prev.ramTotalBytes = 16ULL * 1024 * 1024 * 1024;
  prev.gpuPct = 40.0;
  prev.gpuTempC = 65.0;
  prev.cpuCoreTempC = 65.0;
  prev.rttMs = 20.0;
  prev.downKbps = 1000.0;
  prev.activeConns = 50;
  prev.droppedPktPct = 0.1;
  prev.tcpRetransmitPct = 0.5;
  prev.diskReadMBs = 100.0;
  prev.diskWriteMBs = 50.0;
  prev.processCount = 180;

  telemetry::Snapshot snap = prev;
  snap.snapshotId = 2;
  snap.cpuPct = 95.0;
  snap.cpuCoreTempC = 90.0;

  ScramEngine legacyScram;
  core.ProcessSnapshot(snap, &prev, legacyScram, 1000);

  assert(eventCount > 0);

  auto recent = core.GetEventBus().RecentEvents(100);
  assert(recent.size() > 0);

  core.Shutdown();

  printf("OK\n");
}

inline void TestCollectorBridge() {
  printf("  TestCollectorBridge... ");

  EventBus bus;
  CollectorBridge bridge(bus);

  int eventCount = 0;
  bus.SubscribeAll([&](const SystemEvent&) { eventCount++; });

  bridge.RegisterCollector(L"CPU", [](telemetry::Snapshot& s) {
    s.cpuPct = 75.0;
  });
  bridge.RegisterCollector(L"Memory", [](telemetry::Snapshot& s) {
    s.ramUsedBytes = 10ULL * 1024 * 1024 * 1024;
  });
  bridge.RegisterCollector(L"Network", [](telemetry::Snapshot& s) {
    s.rttMs = 25.0;
  });

  assert(bridge.CollectorCount() == 3);

  auto names = bridge.CollectorNames();
  assert(names.size() == 3);
  assert(names[0] == L"CPU");
  assert(names[1] == L"Memory");
  assert(names[2] == L"Network");

  telemetry::Snapshot snap;
  bridge.CollectAll(snap);

  assert(snap.cpuPct == 75.0);
  assert(snap.ramUsedBytes == 10ULL * 1024 * 1024 * 1024);
  assert(snap.rttMs == 25.0);
  assert(eventCount == 3);

  bridge.NotifyCollectorState(L"CPU", L"DEGRADED");
  assert(eventCount == 4);

  bridge.NotifyError(L"GPU", L"Timeout");
  assert(eventCount == 5);

  printf("OK\n");
}

inline void TestMonixCoreSessions() {
  printf("  TestMonixCoreSessions... ");

  CoreConfig config;
  config.autoStartSession = false;
  MonixCore core(config);

  core.StartSession(L"Manual Session");
  assert(core.GetSessionManager().HasActiveSession());

  telemetry::Snapshot snap;
  snap.snapshotId = 1;
  snap.cpuPct = 50.0;
  snap.ramUsedBytes = 8ULL * 1024 * 1024 * 1024;
  snap.ramTotalBytes = 16ULL * 1024 * 1024 * 1024;
  snap.gpuPct = 40.0;
  snap.gpuTempC = 65.0;
  snap.cpuCoreTempC = 65.0;
  snap.rttMs = 20.0;
  snap.downKbps = 1000.0;
  snap.activeConns = 50;
  snap.droppedPktPct = 0.1;
  snap.tcpRetransmitPct = 0.5;
  snap.diskReadMBs = 100.0;
  snap.diskWriteMBs = 50.0;
  snap.processCount = 180;

  ScramEngine legacyScram;
  core.ProcessSnapshot(snap, nullptr, legacyScram, 1000);

  auto& session = *core.GetSessionManager().CurrentSession();
  assert(session.metadata.totalSnapshots == 1);

  core.PauseSession();
  assert(core.GetSessionManager().IsPaused());

  core.ProcessSnapshot(snap, nullptr, legacyScram, 2000);
  assert(session.metadata.totalSnapshots == 1);

  core.ResumeSession();
  assert(!core.GetSessionManager().IsPaused());

  core.ProcessSnapshot(snap, nullptr, legacyScram, 3000);
  assert(session.metadata.totalSnapshots == 2);

  core.StopSession();
  assert(!core.GetSessionManager().HasActiveSession());

  auto history = core.ListSessions();
  assert(history.size() == 1);

  printf("OK\n");
}

inline void RunFase6Tests() {
  printf("Running FASE 6 tests...\n");
  TestMonixCoreInit();
  TestMonixCoreProcess();
  TestMonixCoreHistory();
  TestMonixCoreEvents();
  TestCollectorBridge();
  TestMonixCoreSessions();
  printf("All FASE 6 tests passed!\n");
}

}
