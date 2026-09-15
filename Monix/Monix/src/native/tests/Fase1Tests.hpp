#pragma once

#include "../core/snapshot/SystemSnapshot.hpp"
#include "../core/snapshot/SnapshotAdapter.hpp"
#include "../events/EventBus.hpp"
#include "../events/EventGenerator.hpp"
#include "../analysis/SnapshotDiffer.hpp"
#include "../telemetry/TelemetryBridge.hpp"

#include <cassert>
#include <cstdio>
#include <string>

namespace monix::tests {

inline void TestSnapshotAdapter() {
  printf("  TestSnapshotAdapter... ");

  Snapshot legacy;
  legacy.snapshotId = 42;
  legacy.collectedAtMonotonicNs = 1000000000ULL;
  legacy.cpuPct = 75.5;
  legacy.ramUsedBytes = 8ULL * 1024 * 1024 * 1024;
  legacy.ramTotalBytes = 16ULL * 1024 * 1024 * 1024;
  legacy.gpuPct = 45.0;
  legacy.gpuTempC = 72.0;
  legacy.cpuCoreTempC = 65.0;
  legacy.fanCount = 3;
  legacy.fanSpeeds = {1200, 1100, 1300};
  legacy.processCount = 180;
  legacy.threadCount = 2500;

  SystemSnapshot snap = SnapshotAdapter::FromLegacy(legacy);

  assert(snap.id == 42);
  assert(snap.timestampNs == 1000000000ULL);
  assert(snap.cpu.pct == 75.5);
  assert(snap.memory.usedBytes == 8ULL * 1024 * 1024 * 1024);
  assert(snap.memory.totalBytes == 16ULL * 1024 * 1024 * 1024);
  assert(snap.gpu.pct == 45.0);
  assert(snap.gpu.tempC == 72.0);
  assert(snap.thermal.cpuCoreTempC == 65.0);
  assert(snap.thermal.fanCount == 3);
  assert(snap.thermal.fanSpeeds.size() == 3);
  assert(snap.thermal.fanSpeeds[0] == 1200);
  assert(snap.processes.count == 180);
  assert(snap.processes.threadCount == 2500);

  printf("OK\n");
}

inline void TestEventBus() {
  printf("  TestEventBus... ");

  EventBus bus;
  int receivedCount = 0;
  EventCategory receivedCategory = EventCategory::System;

  bus.Subscribe(EventCategory::Thermal, [&](const SystemEvent& e) {
    receivedCount++;
    receivedCategory = e.category;
  });

  SystemEvent event;
  event.category = EventCategory::Thermal;
  event.type = L"ThermalWarning";
  event.severity = EventSeverity::High;
  event.timestampNs = 1234567890ULL;
  bus.Emit(std::move(event));

  assert(receivedCount == 1);
  assert(receivedCategory == EventCategory::Thermal);
  assert(bus.TotalEvents() == 1);

  auto recent = bus.RecentEvents(10);
  assert(recent.size() == 1);
  assert(recent[0].type == L"ThermalWarning");

  printf("OK\n");
}

inline void TestSnapshotDiffer() {
  printf("  TestSnapshotDiffer... ");

  SystemSnapshot a;
  a.id = 1;
  a.timestampNs = 1000;
  a.cpu.pct = 45.0;
  a.memory.usedBytes = 8ULL * 1024 * 1024 * 1024;
  a.thermal.cpuCoreTempC = 62.0;
  a.thermal.cpuThrottling = 0;

  SystemSnapshot b;
  b.id = 2;
  b.timestampNs = 2000;
  b.cpu.pct = 92.0;
  b.memory.usedBytes = 12ULL * 1024 * 1024 * 1024;
  b.thermal.cpuCoreTempC = 73.0;
  b.thermal.cpuThrottling = 1;

  SnapshotDiffer differ;
  differ.SetThreshold("thermal.cpuCoreTempC", 70.0, 85.0);

  SnapshotDiff diff = differ.Compute(a, b);

  assert(diff.HasChanges());
  assert(diff.ChangeCount() > 0);
  assert(diff.id == 1);
  assert(diff.snapshotIdBefore == 1);
  assert(diff.snapshotIdAfter == 2);

  bool foundCpu = false;
  bool foundMemory = false;
  bool foundTemp = false;
  bool foundThrottle = false;
  for (const auto& f : diff.fields) {
    if (std::string(f.path).find("cpu.pct") != std::string::npos) foundCpu = true;
    if (std::string(f.path).find("memory.usedBytes") != std::string::npos) foundMemory = true;
    if (std::string(f.path).find("thermal.cpuCoreTempC") != std::string::npos) foundTemp = true;
    if (std::string(f.path).find("thermal.cpuThrottling") != std::string::npos) foundThrottle = true;
  }
  assert(foundCpu);
  assert(foundMemory);
  assert(foundTemp);
  assert(foundThrottle);

  auto highSev = diff.HighSeverityChanges();
  assert(highSev.size() > 0);

  printf("OK\n");
}

inline void TestEventGenerator() {
  printf("  TestEventGenerator... ");

  SystemSnapshot a;
  a.id = 1;
  a.timestampNs = 1000;
  a.cpu.pct = 45.0;

  SystemSnapshot b;
  b.id = 2;
  b.timestampNs = 2000;
  b.cpu.pct = 92.0;

  SnapshotDiffer differ;
  SnapshotDiff diff = differ.Compute(a, b);

  EventGenerator gen;
  auto events = gen.FromDiff(diff, 1, 2);

  assert(!events.empty());

  bool foundCpuEvent = false;
  for (const auto& e : events) {
    if (e.type.find(L"Cpu") != std::wstring::npos) {
      foundCpuEvent = true;
      assert(e.category == EventCategory::Hardware);
      assert(e.subsystem == L"cpu");
      assert(e.timestampNs == 2000);
    }
  }
  assert(foundCpuEvent);

  printf("OK\n");
}

inline void TestTelemetryBridge() {
  printf("  TestTelemetryBridge... ");

  EventBus bus;
  TelemetryBridge bridge(bus);

  int eventCount = 0;
  bus.SubscribeAll([&](const SystemEvent&) { eventCount++; });

  telemetry::ChangeSet changes;
  telemetry::EntityIdentity procId = telemetry::EntityIdentity::Process(1234, 1000);
  changes.AddCreated(procId, nullptr, 5000000000ULL);

  bridge.OnChangeSet(changes, 5000000000ULL);
  assert(eventCount == 1);

  bridge.OnScramFinding(L"Thermal warning", L"CPU temp high", 75, 6000000000ULL);
  assert(eventCount == 2);

  bridge.OnCollectorStateChange(L"ThermalCollector", L"DEGRADED", 7000000000ULL);
  assert(eventCount == 3);

  auto thermalEvents = bus.EventsByCategory(EventCategory::Scram);
  assert(thermalEvents.size() == 1);
  assert(thermalEvents[0].type == L"ScramFindingEmitted");

  printf("OK\n");
}

inline void TestProcessDiff() {
  printf("  TestProcessDiff... ");

  SystemSnapshot a;
  a.id = 1;
  a.timestampNs = 1000;
  ProcessInfo p1;
  p1.name = L"chrome.exe";
  p1.pid = 100;
  p1.cpuPct = 25.0;
  p1.ramBytes = 500000000;
  a.processes.list.push_back(p1);
  a.processes.count = 1;

  SystemSnapshot b;
  b.id = 2;
  b.timestampNs = 2000;
  ProcessInfo p2;
  p2.name = L"chrome.exe";
  p2.pid = 100;
  p2.cpuPct = 45.0;
  p2.ramBytes = 800000000;
  b.processes.list.push_back(p2);
  ProcessInfo p3;
  p3.name = L"game.exe";
  p3.pid = 200;
  p3.cpuPct = 80.0;
  b.processes.list.push_back(p3);
  b.processes.count = 2;

  SnapshotDiffer differ;
  SnapshotDiff diff = differ.Compute(a, b);

  bool foundStarted = false;
  bool foundStopped = false;
  for (const auto& f : diff.fields) {
    if (std::string(f.path) == "processes.started") foundStarted = true;
    if (std::string(f.path) == "processes.stopped") foundStopped = true;
  }
  assert(foundStarted);
  assert(!foundStopped);

  EventGenerator gen;
  auto events = gen.FromDiff(diff, 1, 2);

  bool foundProcessStarted = false;
  for (const auto& e : events) {
    if (e.type == L"ProcessStarted") {
      foundProcessStarted = true;
      assert(e.category == EventCategory::Process);
    }
  }
  assert(foundProcessStarted);

  printf("OK\n");
}

inline void RunAllTests() {
  printf("Running FASE 1 tests...\n");
  TestSnapshotAdapter();
  TestEventBus();
  TestSnapshotDiffer();
  TestEventGenerator();
  TestTelemetryBridge();
  TestProcessDiff();
  printf("All FASE 1 tests passed!\n");
}

}
