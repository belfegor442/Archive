#pragma once

#include "../core/Timeline.hpp"
#include "../core/SessionManager.hpp"
#include "../core/TimelineBridge.hpp"
#include "../events/EventBus.hpp"
#include "../storage/SessionPersistence.hpp"

#include <cassert>
#include <cstdio>
#include <string>

namespace monix::tests {

inline void TestTimeline() {
  printf("  TestTimeline... ");

  Timeline tl;
  assert(tl.Empty());
  assert(tl.TotalEntries() == 0);

  tl.AddSnapshot(1000000, 1, L"Snapshot #1");
  tl.AddSnapshot(2000000, 2, L"Snapshot #2");
  assert(tl.TotalEntries() == 2);

  auto range = tl.EntriesInRange(0, 1500000);
  assert(range.size() == 1);
  assert(range[0].snapshotId == 1);

  SystemEvent evt;
  evt.timestampNs = 1500000;
  evt.type = L"CpuHigh";
  evt.category = EventCategory::Hardware;
  evt.severity = EventSeverity::High;
  evt.description = L"CPU 95%";
  tl.AddEvent(evt);
  assert(tl.TotalEntries() == 3);

  auto hwEntries = tl.EntriesByCategory(EventCategory::Hardware);
  assert(hwEntries.size() == 1);
  assert(hwEntries[0].type == L"CpuHigh");

  tl.AddFinding(101, L"Thermal Warning", 65, 3000000);
  assert(tl.TotalEntries() == 4);

  auto highSev = tl.EntriesBySeverity(EventSeverity::High);
  assert(highSev.size() >= 2);

  auto recent = tl.RecentEntries(2);
  assert(recent.size() == 2);

  auto ctx = tl.ContextAround(2, 1, 1);
  assert(ctx.size() == 3);

  auto found = tl.Search(L"CpuHigh");
  assert(found.size() == 1);

  auto counts = tl.EntriesByCategoryCount();
  assert(counts[EventCategory::Hardware] == 1);
  assert(counts[EventCategory::System] == 2);

  tl.Clear();
  assert(tl.Empty());

  printf("OK\n");
}

inline void TestSessionManager() {
  printf("  TestSessionManager... ");

  SessionManager sm;
  assert(!sm.HasActiveSession());
  assert(sm.CurrentSession() == nullptr);

  auto& session = sm.StartSession(L"Test Session");
  assert(sm.HasActiveSession());
  assert(!sm.IsPaused());
  assert(sm.CurrentSession() != nullptr);
  assert(session.metadata.name == L"Test Session");
  assert(!session.metadata.id.empty());

  SystemSnapshot snap;
  snap.id = 1;
  snap.timestampNs = 1000;
  snap.cpu.pct = 75.0;
  sm.RecordSnapshot(snap);
  assert(session.metadata.totalSnapshots == 1);

  SystemEvent event;
  event.type = L"TestEvent";
  event.severity = EventSeverity::Warning;
  sm.RecordEvent(event);
  assert(session.metadata.totalEvents == 1);
  assert(session.warningCount == 1);

  sm.PauseSession();
  assert(sm.IsPaused());

  sm.RecordSnapshot(snap);
  assert(session.metadata.totalSnapshots == 1);

  sm.ResumeSession();
  assert(!sm.IsPaused());

  sm.RecordSnapshot(snap);
  assert(session.metadata.totalSnapshots == 2);

  sm.StopSession();
  assert(!sm.HasActiveSession());
  assert(sm.CurrentSession() == nullptr);
  assert(sm.History().size() == 1);
  assert(sm.History()[0].totalSnapshots == 2);

  auto& s2 = sm.StartSession(L"Session 2");
  SystemSnapshot snap2;
  snap2.id = 10;
  snap2.cpu.pct = 30.0;
  sm.RecordSnapshot(snap2);
  sm.StopSession();

  assert(sm.History().size() == 2);

  printf("OK\n");
}

inline void TestSessionComparison() {
  printf("  TestSessionComparison... ");

  SessionManager sm;

  auto& s1 = sm.StartSession(L"Session A");
  SystemSnapshot snap1;
  snap1.id = 1;
  snap1.cpu.pct = 80.0;
  sm.RecordSnapshot(snap1);
  sm.StopSession();

  auto& s2 = sm.StartSession(L"Session B");
  SystemSnapshot snap2;
  snap2.id = 2;
  snap2.cpu.pct = 40.0;
  sm.RecordSnapshot(snap2);
  sm.StopSession();

  auto comp = SessionManager::CompareSessions(s1, s2);
  assert(comp.avgCpuDiff == 40.0);
  assert(!comp.summary.empty());

  printf("OK\n");
}

inline void TestTimelineBridge() {
  printf("  TestTimelineBridge... ");

  EventBus bus;
  Timeline tl;
  SessionManager sm;
  TimelineBridge bridge(bus, tl, sm);

  int tlCount = 0;
  bus.SubscribeAll([&](const SystemEvent&) { tlCount++; });

  bridge.OnSnapshot(1000000, 1);
  assert(tl.TotalEntries() == 1);
  assert(tlCount == 0);

  bridge.OnFinding(101, L"Test Finding", 60, 2000000);
  assert(tl.TotalEntries() == 2);

  bridge.OnUserAction(L"Button clicked", 3000000);
  assert(tl.TotalEntries() == 3);

  bridge.OnDiagnostic(L"Memory", L"High usage", 2, 4000000);
  assert(tl.TotalEntries() == 4);

  assert(!bridge.IsRecording());
  bridge.StartRecording();
  assert(bridge.IsRecording());

  SystemEvent event;
  event.type = L"TestEvent";
  event.severity = EventSeverity::Info;
  bus.Emit(std::move(event));
  assert(tl.TotalEntries() == 5);
  assert(tlCount == 1);

  auto& session = bridge.StartSession(L"Bridge Test");
  assert(session.metadata.name == L"Bridge Test");

  SystemEvent event2;
  event2.type = L"AnotherEvent";
  event2.severity = EventSeverity::Warning;
  bus.Emit(std::move(event2));
  assert(session.metadata.totalEvents == 1);

  bridge.StopRecording();
  SystemEvent event3;
  event3.type = L"IgnoredEvent";
  bus.Emit(std::move(event3));
  assert(session.metadata.totalEvents == 1);

  bridge.StopSession();
  assert(!bridge.HasActiveSession());

  printf("OK\n");
}

inline void TestTimelinePersistence() {
  printf("  TestTimelinePersistence... ");

  std::wstring testPath = L"test_sessions";
  SessionPersistence persist(testPath);

  Session session;
  session.metadata.id = L"TEST-001";
  session.metadata.name = L"Test Session";
  session.metadata.startTimeNs = 1000000000ULL;
  session.metadata.endTimeNs = 2000000000ULL;
  session.metadata.totalSnapshots = 5;
  session.metadata.totalEvents = 3;
  session.metadata.totalFindings = 2;
  session.metadata.peakRiskScore = 75;

  SystemSnapshot snap;
  snap.id = 1;
  snap.timestampNs = 1000000000ULL;
  snap.cpu.pct = 55.0;
  snap.memory.usedBytes = 8ULL * 1024 * 1024 * 1024;
  snap.memory.totalBytes = 16ULL * 1024 * 1024 * 1024;
  snap.gpu.pct = 42.0;
  snap.gpu.pctValid = 1;
  snap.thermal.cpuCoreTempC = 68.0;
  snap.thermal.fanCount = 3;
  snap.thermal.fanSpeeds = {1200, 1100, 1300};
  snap.network.upKbps = 500.0;
  snap.network.downKbps = 2000.0;
  snap.network.rttMs = 25.0;
  snap.storage.diskReadMBs = 150.0;
  snap.storage.diskWriteMBs = 80.0;
  snap.storage.iops = 500.0;
  snap.storage.queueDepth = 2.5;
  snap.storage.activeTimePct = 35.0;
  snap.storage.ready = 1;
  snap.storage.wearPct = 15.0;
  snap.processes.count = 180;
  snap.processes.threadCount = 2500;
  snap.processes.handleCount = 50000;
  snap.processes.ramUsedBytes = 4ULL * 1024 * 1024 * 1024;
  snap.processes.cpuPct = 35.0;
  snap.power.batteryPresent = 1;
  snap.power.batteryChargePct = 85.0;
  snap.power.batteryChargeCycles = 150;
  snap.power.acConnected = 1;
  snap.reliability.biosHealthStatus = 1;
  snap.reliability.bootHealthStatus = 1;
  snap.security.defenderRealtimePct = 100.0;
  snap.security.defenderScanStatus = 1;
  snap.security.rdpEnabled = 0;
  session.snapshots.push_back(snap);

  SystemEvent event;
  event.id = 1;
  event.timestampNs = 1000000000ULL;
  event.category = EventCategory::Thermal;
  event.severity = EventSeverity::Medium;
  event.type = L"ThermalWarning";
  event.subsystem = L"thermal";
  event.description = L"CPU temp 72C";
  event.processId = 0;
  event.uiActionable = 1;
  session.events.push_back(event);

  bool saved = persist.SaveSession(session);
  assert(saved);

  auto sessions = persist.ListSessions();
  assert(sessions.size() >= 1);

  bool deleted = persist.DeleteSession(L"TEST-001");
  assert(deleted);

  std::filesystem::remove_all(testPath);

  printf("OK\n");
}

inline void RunFase2Tests() {
  printf("Running FASE 2 tests...\n");
  TestTimeline();
  TestSessionManager();
  TestSessionComparison();
  TestTimelineBridge();
  TestTimelinePersistence();
  printf("All FASE 2 tests passed!\n");
}

}
