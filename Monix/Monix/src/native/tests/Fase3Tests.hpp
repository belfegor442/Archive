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
    snap.network.rttMs = 20.0;
    snap.network.downKbps = 1000.0;
    snap.storage.diskReadMBs = 100.0;
    snap.storage.diskWriteMBs = 50.0;
    snap.processes.count = 180;

    detector.RecordBaseline(L"cpu.pct", snap.cpu.pct);
    detector.RecordBaseline(L"memory.usedPct", snap.memory.UsedPct());
    detector.RecordBaseline(L"gpu.pct", snap.gpu.pct);
    detector.RecordBaseline(L"thermal.cpuCoreTempC", snap.thermal.cpuCoreTempC);
    detector.RecordBaseline(L"network.rttMs", snap.network.rttMs);
    detector.RecordBaseline(L"network.downKbps", snap.network.downKbps);
    detector.RecordBaseline(L"storage.diskReadMBs", snap.storage.diskReadMBs);
    detector.RecordBaseline(L"storage.diskWriteMBs", snap.storage.diskWriteMBs);
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
  spike.network.rttMs = 20.0;
  spike.network.downKbps = 1000.0;
  spike.storage.diskReadMBs = 100.0;
  spike.storage.diskWriteMBs = 50.0;
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
  snap.network.rttMs = 20.0;
  snap.network.droppedPktPct = 0.1;
  snap.network.tcpRetransmitPct = 0.5;
  snap.storage.diskReadMBs = 100.0;
  snap.storage.diskWriteMBs = 50.0;
  snap.storage.totalBytes = 500ULL * 1024 * 1024 * 1024;
  snap.storage.freeBytes = 200ULL * 1024 * 1024 * 1024;
  snap.storage.ready = 1;
  snap.storage.wearPct = 10.0;
  snap.security.defenderRealtimePct = 100.0;
  snap.security.defenderScanStatus = 1;
  snap.security.defenderThreatsDetected = 0;
  snap.security.rdpEnabled = 0;
  snap.security.sshEnabled = 0;
  snap.security.failedLoginAttempts = 0;
  snap.reliability.crashDumpDetected = 0;
  snap.reliability.hardwareErrors = 0;
  snap.reliability.storageErrors = 0;
  snap.power.batteryPresent = 0;

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
    snap.network.rttMs = 20.0;
    snap.network.droppedPktPct = 0.1;
    snap.network.tcpRetransmitPct = 0.5;
    snap.network.downKbps = 1000.0;
    snap.storage.diskReadMBs = 100.0;
    snap.storage.diskWriteMBs = 50.0;
    snap.storage.totalBytes = 500ULL * 1024 * 1024 * 1024;
    snap.storage.freeBytes = 200ULL * 1024 * 1024 * 1024;
    snap.storage.ready = 1;
    snap.storage.wearPct = 10.0;
    snap.security.defenderRealtimePct = 100.0;
    snap.security.defenderScanStatus = 1;
    snap.security.defenderThreatsDetected = 0;
    snap.security.rdpEnabled = 0;
    snap.security.sshEnabled = 0;
    snap.security.failedLoginAttempts = 0;
    snap.reliability.crashDumpDetected = 0;
    snap.reliability.hardwareErrors = 0;
    snap.reliability.storageErrors = 0;
    snap.power.batteryPresent = 0;
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
  good.network.rttMs = 20.0;
  good.network.droppedPktPct = 0.1;
  good.network.tcpRetransmitPct = 0.5;
  good.network.downKbps = 1000.0;
  good.storage.diskReadMBs = 100.0;
  good.storage.diskWriteMBs = 50.0;
  good.storage.totalBytes = 500ULL * 1024 * 1024 * 1024;
  good.storage.freeBytes = 200ULL * 1024 * 1024 * 1024;
  good.storage.ready = 1;
  good.storage.wearPct = 10.0;
  good.security.defenderRealtimePct = 100.0;
  good.security.defenderScanStatus = 1;
  good.security.defenderThreatsDetected = 0;
  good.security.rdpEnabled = 0;
  good.security.sshEnabled = 0;
  good.security.failedLoginAttempts = 0;
  good.reliability.crashDumpDetected = 0;
  good.reliability.hardwareErrors = 0;
  good.reliability.storageErrors = 0;
  good.power.batteryPresent = 0;

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
  bad.network.rttMs = 500.0;
  bad.network.droppedPktPct = 10.0;
  bad.network.tcpRetransmitPct = 5.0;
  bad.network.downKbps = 10000.0;
  bad.storage.diskReadMBs = 1000.0;
  bad.storage.diskWriteMBs = 1000.0;
  bad.storage.totalBytes = 500ULL * 1024 * 1024 * 1024;
  bad.storage.freeBytes = 20ULL * 1024 * 1024 * 1024;
  bad.storage.ready = 1;
  bad.storage.wearPct = 90.0;
  bad.security.defenderRealtimePct = 0.0;
  bad.security.defenderScanStatus = 0;
  bad.security.defenderThreatsDetected = 3;
  bad.security.rdpEnabled = 1;
  bad.security.sshEnabled = 1;
  bad.security.failedLoginAttempts = 10;
  bad.reliability.crashDumpDetected = 1;
  bad.reliability.lastBugCheckCode = 0x0000007E;
  bad.reliability.hardwareErrors = 5;
  bad.reliability.storageErrors = 2;
  bad.power.batteryPresent = 0;

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
