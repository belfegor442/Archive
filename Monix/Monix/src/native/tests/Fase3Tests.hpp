#pragma once

#include "../core/correlation/CorrelationEngine.hpp"
#include "../core/anomaly/AnomalyDetector.hpp"
#include "../core/scram/ScramEngine2.hpp"
#include "../core/diagnostics/DiagnosticsEngine.hpp"
#include "../core/Timeline.hpp"
#include "../events/EventBus.hpp"

#include <cassert>
#include <cstdio>

namespace monix::tests {

inline void TestCorrelationEngine() {
  printf("  TestCorrelationEngine... ");

  CorrelationEngine engine;
  assert(engine.SignalCount() == 0);

  CorrelatedSignal s1;
  s1.timestampNs = 1000000;
  s1.source = L"cpu";
  s1.signalType = L"cpu_spike";
  s1.value = 95.0;
  s1.category = EventCategory::Hardware;
  s1.severity = EventSeverity::High;
  engine.AddSignal(s1);

  CorrelatedSignal s2;
  s2.timestampNs = 1200000;
  s2.source = L"network";
  s2.signalType = L"network_spike";
  s2.value = 5000.0;
  s2.category = EventCategory::Network;
  s2.severity = EventSeverity::Medium;
  engine.AddSignal(s2);

  CorrelatedSignal s3;
  s3.timestampNs = 1400000;
  s3.source = L"thermal";
  s3.signalType = L"thermal_spike";
  s3.value = 85.0;
  s3.category = EventCategory::Thermal;
  s3.severity = EventSeverity::High;
  engine.AddSignal(s3);

  assert(engine.SignalCount() == 3);

  auto recent = engine.RecentSignals(10);
  assert(recent.size() == 3);

  auto hwSignals = engine.SignalsByCategory(EventCategory::Hardware);
  assert(hwSignals.size() == 1);

  auto cpuSignals = engine.SignalsByType(L"cpu_spike");
  assert(cpuSignals.size() == 1);

  auto range = engine.SignalsInRange(900000, 1300000);
  assert(range.size() == 2);

  auto cluster = engine.FindCluster(900000, 1500000);
  assert(cluster.signals.size() == 3);
  assert(cluster.confidence > 0.5);
  assert(!cluster.pattern.empty());
  assert(!cluster.summary.empty());

  auto clusters = engine.DetectClusters(2000000);
  assert(clusters.size() > 0);

  auto counts = engine.SignalTypeCounts();
  assert(counts[L"cpu_spike"] == 1);
  assert(counts[L"network_spike"] == 1);
  assert(counts[L"thermal_spike"] == 1);

  engine.Clear();
  assert(engine.SignalCount() == 0);

  printf("OK\n");
}

inline void TestAnomalyDetector() {
  printf("  TestAnomalyDetector... ");

  AnomalyDetector detector;

  for (int i = 0; i < 20; ++i) {
    SystemSnapshot snap;
    snap.id = i;
    snap.cpu.pct = 50.0 + (i % 3);
    snap.memory.usedBytes = 8ULL * 1024 * 1024 * 1024;
    snap.memory.totalBytes = 16ULL * 1024 * 1024 * 1024;
    snap.gpu.pct = 40.0;
    snap.gpu.pctValid = 1;
    snap.thermal.cpuCoreTempC = 65.0 + (i % 3);
    snap.network.pingRttMs = 20;
    snap.network.downBytesPerSec = 1000ULL * 1024;
    snap.storage.readBytesPerSec = 100ULL * 1024 * 1024;
    snap.storage.writeBytesPerSec = 50ULL * 1024 * 1024;
    snap.processes.count = 180;

    detector.RecordBaseline(L"cpu.pct", snap.cpu.pct);
    detector.RecordBaseline(L"memory.usedPct", snap.memory.usedBytes * 100.0 / snap.memory.totalBytes);
    detector.RecordBaseline(L"gpu.pct", snap.gpu.pct);
    detector.RecordBaseline(L"thermal.cpuCoreTempC", snap.thermal.cpuCoreTempC);
    detector.RecordBaseline(L"network.pingRttMs", snap.network.pingRttMs);
    detector.RecordBaseline(L"network.downBytesPerSec", snap.network.downBytesPerSec);
    detector.RecordBaseline(L"storage.readBytesPerSec", snap.storage.readBytesPerSec);
    detector.RecordBaseline(L"storage.writeBytesPerSec", snap.storage.writeBytesPerSec);
    detector.RecordBaseline(L"processes.count", static_cast<double>(snap.processes.count));
    detector.RecordSnapshot(snap);
  }

  auto cpuBaseline = detector.GetBaseline(L"cpu.pct");
  assert(cpuBaseline.count == 20);
  assert(cpuBaseline.Mean() > 49.0);

  SystemSnapshot spike;
  spike.id = 21;
  spike.cpu.pct = 98.0;
  spike.memory.usedBytes = 8ULL * 1024 * 1024 * 1024;
  spike.memory.totalBytes = 16ULL * 1024 * 1024 * 1024;
  spike.gpu.pct = 40.0;
  spike.gpu.pctValid = 1;
  spike.thermal.cpuCoreTempC = 65.0;
  spike.network.pingRttMs = 20;
  spike.network.downBytesPerSec = 1000ULL * 1024;
  spike.storage.readBytesPerSec = 100ULL * 1024 * 1024;
  spike.storage.writeBytesPerSec = 50ULL * 1024 * 1024;
  spike.processes.count = 180;

  auto anomalies = detector.Detect(spike, 22000000);
  assert(!anomalies.empty());

  bool foundCpuAnomaly = false;
  for (const auto& a : anomalies) {
    if (a.metricName == L"cpu.pct" && a.anomalyType == L"spike") {
      foundCpuAnomaly = true;
      assert(a.deviationScore > 3.0);
      assert(a.severity >= EventSeverity::High);
    }
  }
  assert(foundCpuAnomaly);

  detector.Clear();
  assert(detector.GetBaseline(L"cpu.pct").count == 0);

  printf("OK\n");
}

inline void TestScramEngine2() {
  printf("  TestScramEngine2... ");

  ScramEngine2 engine;
  assert(engine.RuleCount() >= 8);
  assert(!engine.IsCalibrated());

  SystemSnapshot snap;
  snap.id = 1;
  snap.cpu.pct = 95.0;
  snap.memory.usedBytes = 8ULL * 1024 * 1024 * 1024;
  snap.memory.totalBytes = 16ULL * 1024 * 1024 * 1024;
  snap.gpu.pct = 40.0;
  snap.gpu.pctValid = 1;
  snap.thermal.cpuCoreTempC = 65.0;
  snap.thermal.cpuThrottling = 0;
  snap.network.pingRttMs = 20;
  snap.storage.readBytesPerSec = 100ULL * 1024 * 1024;
  snap.storage.writeBytesPerSec = 50ULL * 1024 * 1024;
  snap.storage.totalBytes = 500ULL * 1024 * 1024 * 1024;
  snap.storage.freeBytes = 200ULL * 1024 * 1024 * 1024;
  snap.security.unsignedDriverCount = 0;
  snap.security.suspiciousScriptHosts = 0;
  snap.security.lsassAccessCount = 0;
  snap.security.debugPortActive = 0;
  snap.reliability.crashEventsToday = 0;
  snap.reliability.unhandledExceptionCount = 0;
  snap.reliability.heapCorruptionDetected = 0;
  snap.power.batteryFlag = 128;

  auto findings = engine.Evaluate(snap, nullptr, nullptr, 1000000);
  assert(findings.empty());
  assert(!engine.IsCalibrated());

  for (int i = 0; i < 5; ++i) {
    snap.id = i + 2;
    engine.Evaluate(snap, nullptr, nullptr, 1000000 + (i + 1) * 1000000);
  }
  assert(engine.IsCalibrated());

  findings = engine.Evaluate(snap, nullptr, nullptr, 6000000);
  assert(findings.empty());

  findings = engine.Evaluate(snap, nullptr, nullptr, 7000000);
  assert(findings.size() >= 1);

  bool foundCpu = false;
  for (const auto& f : findings) {
    if (f.ruleId == L"CPU_OVERLOAD") {
      foundCpu = true;
      assert(f.riskScore == 60);
      assert(!f.headline.empty());
      assert(!f.insight.empty());
    }
  }
  assert(foundCpu);

  findings = engine.Evaluate(snap, nullptr, nullptr, 8000000);
  assert(findings.empty());

  engine.ClearState();
  engine.ResetCalibration();
  assert(!engine.IsCalibrated());

  printf("OK\n");
}

inline void TestDiagnosticsEngine() {
  printf("  TestDiagnosticsEngine... ");

  DiagnosticsEngine diag;
  CorrelationEngine corr;
  AnomalyDetector anomaly;

  for (int i = 0; i < 10; ++i) {
    SystemSnapshot snap;
    snap.id = i;
    snap.cpu.pct = 50.0;
    snap.memory.usedBytes = 8ULL * 1024 * 1024 * 1024;
    snap.memory.totalBytes = 16ULL * 1024 * 1024 * 1024;
    snap.gpu.pct = 40.0;
    snap.gpu.pctValid = 1;
    snap.thermal.cpuCoreTempC = 65.0;
    snap.network.pingRttMs = 20;
    snap.network.downBytesPerSec = 1000ULL * 1024;
    snap.storage.readBytesPerSec = 100ULL * 1024 * 1024;
    snap.storage.writeBytesPerSec = 50ULL * 1024 * 1024;
    snap.storage.totalBytes = 500ULL * 1024 * 1024 * 1024;
    snap.storage.freeBytes = 200ULL * 1024 * 1024 * 1024;
    snap.security.unsignedDriverCount = 0;
    snap.security.suspiciousScriptHosts = 0;
    snap.security.lsassAccessCount = 0;
    snap.security.debugPortActive = 0;
    snap.reliability.crashEventsToday = 0;
    snap.reliability.unhandledExceptionCount = 0;
    snap.reliability.heapCorruptionDetected = 0;
    snap.power.batteryFlag = 128;
    anomaly.RecordSnapshot(snap);
  }

  SystemSnapshot good;
  good.id = 11;
  good.cpu.pct = 50.0;
  good.memory.usedBytes = 8ULL * 1024 * 1024 * 1024;
  good.memory.totalBytes = 16ULL * 1024 * 1024 * 1024;
  good.gpu.pct = 40.0;
  good.gpu.pctValid = 1;
  good.thermal.cpuCoreTempC = 65.0;
  good.network.pingRttMs = 20;
  good.network.downBytesPerSec = 1000ULL * 1024;
  good.storage.readBytesPerSec = 100ULL * 1024 * 1024;
  good.storage.writeBytesPerSec = 50ULL * 1024 * 1024;
  good.storage.totalBytes = 500ULL * 1024 * 1024 * 1024;
  good.storage.freeBytes = 200ULL * 1024 * 1024 * 1024;
  good.security.unsignedDriverCount = 0;
  good.security.suspiciousScriptHosts = 0;
  good.security.lsassAccessCount = 0;
  good.security.debugPortActive = 0;
  good.reliability.crashEventsToday = 0;
  good.reliability.unhandledExceptionCount = 0;
  good.reliability.heapCorruptionDetected = 0;
  good.power.batteryFlag = 128;

  auto report = diag.Run(good, corr, anomaly, 12000000);
  assert(report.totalChecks >= 7);
  assert(report.passedChecks >= 5);
  assert(report.overallScore > 50);
  assert(!report.checks.empty());

  SystemSnapshot bad;
  bad.id = 12;
  bad.cpu.pct = 98.0;
  bad.memory.usedBytes = 15ULL * 1024 * 1024 * 1024;
  bad.memory.totalBytes = 16ULL * 1024 * 1024 * 1024;
  bad.gpu.pct = 99.0;
  bad.gpu.pctValid = 1;
  bad.thermal.cpuCoreTempC = 92.0;
  bad.network.pingRttMs = 500;
  bad.network.downBytesPerSec = 10000ULL * 1024;
  bad.storage.readBytesPerSec = 1000ULL * 1024 * 1024;
  bad.storage.writeBytesPerSec = 1000ULL * 1024 * 1024;
  bad.storage.totalBytes = 500ULL * 1024 * 1024 * 1024;
  bad.storage.freeBytes = 20ULL * 1024 * 1024 * 1024;
  bad.security.unsignedDriverCount = 5;
  bad.security.suspiciousScriptHosts = 3;
  bad.security.lsassAccessCount = 10;
  bad.security.debugPortActive = 1;
  bad.reliability.crashEventsToday = 1;
  bad.reliability.unhandledExceptionCount = 5;
  bad.reliability.heapCorruptionDetected = 1;
  bad.power.batteryFlag = 128;

  auto badReport = diag.Run(bad, corr, anomaly, 13000000);
  assert(badReport.failedChecks > 0);
  assert(badReport.overallScore < report.overallScore);

  printf("OK\n");
}

inline void RunFase3Tests() {
  printf("Running FASE 3 tests...\n");
  TestCorrelationEngine();
  TestAnomalyDetector();
  TestScramEngine2();
  TestDiagnosticsEngine();
  printf("All FASE 3 tests passed!\n");
}

}
