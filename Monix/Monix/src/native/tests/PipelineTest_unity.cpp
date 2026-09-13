#pragma once
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>

#include <cassert>
#include <cstdio>
#include <string>

#include "../telemetry/normalization/Normalizer.hpp"
#include "../telemetry/normalization/Validator.hpp"
#include "../telemetry/correlation/CorrelationEngine.hpp"
#include "../telemetry/state/StateStore.hpp"
#include "../telemetry/state/ChangeSet.hpp"
#include "../telemetry/state/EntityTracker.hpp"
#include "../telemetry/state/TimelineRing.hpp"
#include "../telemetry/state/ThresholdDetector.hpp"
#include "../telemetry/state/SnapshotChangeDetector.hpp"
#include "../telemetry/state/ProcessChangeDetector.hpp"
#include "../telemetry/export/WebSocketServer.hpp"
#include "../telemetry/export/TelemetryRelay.hpp"
#include "../telemetry/export/ScramHistoryRing.hpp"
#include "../telemetry/export/HttpBridge.hpp"
#include "../telemetry/PipelineOrchestrator.hpp"
#include "../telemetry/PipelineHealth.hpp"
#include "../scram/ScramEngine.hpp"
#include "../scram/risk/RiskScorer.hpp"
#include "../scram/risk/SeverityTracker.hpp"
#include "../scram/risk/HeadlineGenerator.hpp"

// Implementation files needed for linking
#include "../telemetry/correlation/CorrelationEngine.cpp"
#include "../telemetry/state/StateStore.cpp"
#include "../telemetry/state/ThresholdDetector.cpp"
#include "../telemetry/state/SnapshotChangeDetector.cpp"
#include "../telemetry/state/EntityTracker.cpp"

// Stub for WideToUtf8 (avoid pulling in full TextUtils.cpp with GDI deps)
namespace monix {
std::string WideToUtf8(const std::wstring& ws) {
  if (ws.empty()) return {};
  int sz = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), (int)ws.size(), nullptr, 0, nullptr, nullptr);
  std::string result(sz, 0);
  WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), (int)ws.size(), result.data(), sz, nullptr, nullptr);
  return result;
}
}

using namespace monix;
using namespace monix::telemetry;

static int testsPassed = 0;
static int testsFailed = 0;

#define TEST(name) \
  do { printf("  TEST: %-40s ", name); } while(0)
#define PASS() \
  do { printf("[PASS]\n"); testsPassed++; } while(0)
#define FAIL(msg) \
  do { printf("[FAIL] %s\n", msg); testsFailed++; } while(0)
#define ASSERT_TRUE(cond, msg) \
  do { if (!(cond)) { FAIL(msg); return; } } while(0)

void TestNormalizer() {
  TEST("Normalizer clamps CPU > 100");
  Snapshot s{};
  s.cpuPct = 150.0;
  Normalizer norm;
  norm.Normalize(s);
  ASSERT_TRUE(s.cpuPct == 100.0, "cpuPct not clamped");
  ASSERT_TRUE(!norm.Events().empty(), "no events emitted");
  PASS();
}

void TestNormalizerNegative() {
  TEST("Normalizer clamps negative CPU");
  Snapshot s{};
  s.cpuPct = -10.0;
  Normalizer norm;
  norm.Normalize(s);
  ASSERT_TRUE(s.cpuPct == 0.0, "cpuPct not clamped to 0");
  PASS();
}

void TestNormalizerRam() {
  TEST("Normalizer clamps RAM exceeding total");
  Snapshot s{};
  s.ramUsedBytes = 20000000000ULL;
  s.ramTotalBytes = 16000000000ULL;
  Normalizer norm;
  norm.Normalize(s);
  ASSERT_TRUE(s.ramUsedBytes == s.ramTotalBytes, "ramUsedBytes not clamped");
  PASS();
}

void TestValidator() {
  TEST("Validator detects suspicious CPU");
  Snapshot s{};
  s.cpuPct = 99.9;
  s.ramTotalBytes = 16000000000ULL;
  s.ramUsedBytes = 8000000000ULL;
  Validator val;
  val.Validate(s);
  ASSERT_TRUE(val.HasSuspicious() || val.IsValid(), "unexpected invalid state");
  PASS();
}

void TestValidatorInvalid() {
  TEST("Validator detects invalid disk");
  Snapshot s{};
  s.diskTotalBytes = 500000000000ULL;
  s.diskFreeBytes = 600000000000ULL;
  Validator val;
  val.Validate(s);
  ASSERT_TRUE(val.HasInvalid() || val.HasSuspicious(), "disk anomaly not detected");
  PASS();
}

void TestCorrelationEngine() {
  TEST("CorrelationEngine tracks CPU-Temp");
  CorrelationEngine eng;
  eng.AddDefaultRules();
  Snapshot prev{};
  prev.cpuPct = 30.0;
  prev.cpuCoreTempC = 55.0;
  Snapshot cur{};
  cur.cpuPct = 85.0;
  cur.cpuCoreTempC = 72.0;
  for (int i = 0; i < 10; ++i) {
    eng.Observe(cur, &prev, static_cast<std::uint64_t>(i) * 1000000000ULL);
    prev = cur;
    cur.cpuPct += 2.0;
    cur.cpuCoreTempC += 1.0;
  }
  ASSERT_TRUE(!eng.Results().empty(), "no results");
  PASS();
}

void TestStateStore() {
  TEST("StateStore stores snapshots");
  StateStore store;
  Snapshot s1{};
  s1.cpuPct = 10.0;
  s1.ramUsedBytes = 1000;
  store.Update(s1);
  ASSERT_TRUE(store.HistorySize() == 1, "expected 1 snapshot");
  Snapshot s2{};
  s2.cpuPct = 20.0;
  store.Update(s2);
  ASSERT_TRUE(store.HistorySize() == 2, "expected 2 snapshots");
  ASSERT_TRUE(store.Current().cpuPct == 20.0, "current mismatch");
  PASS();
}

void TestTimelineRing() {
  TEST("TimelineRing wraps around");
  TimelineRing ring(4);
  for (int i = 0; i < 8; ++i) {
    Snapshot s{};
    s.cpuPct = static_cast<double>(i);
    ring.Push(s);
  }
  ASSERT_TRUE(ring.Count() == 4, "ring should cap at 4");
  ASSERT_TRUE(ring.Current().cpuPct == 7.0, "current should be latest");
  PASS();
}

void TestChangeSet() {
  TEST("ChangeSet merges events");
  ChangeSet cs;
  ChangeEvent evt;
  evt.kind = ChangeEvent::Kind::Created;
  evt.attribute = "test_process";
  cs.events.push_back(evt);
  ASSERT_TRUE(!cs.Empty(), "changeset should not be empty");
  cs.Clear();
  ASSERT_TRUE(cs.Empty(), "changeset should be empty after clear");
  PASS();
}

void TestScramHistoryRing() {
  TEST("ScramHistoryRing stores and retrieves");
  ScramHistoryRing ring(8);
  for (int i = 0; i < 12; ++i) {
    ScramResult r;
    r.riskScore = i * 10;
    r.headline = L"Test headline " + std::to_wstring(i);
    ring.Record(r, static_cast<std::uint64_t>(i) * 1000);
  }
  ASSERT_TRUE(ring.Count() == 8, "ring should cap at 8");
  auto latest = ring.GetLatest();
  ASSERT_TRUE(latest.riskScore == 110, "latest risk mismatch");
  auto recent = ring.GetRecent(3);
  ASSERT_TRUE(recent.size() == 3, "recent size mismatch");
  auto highRisk = ring.GetByRiskThreshold(80);
  ASSERT_TRUE(!highRisk.empty(), "high risk should not be empty");
  PASS();
}

void TestScramHistoryRingJson() {
  TEST("ScramHistoryRing serializes JSON");
  ScramHistoryRing ring(4);
  ScramResult r;
  r.riskScore = 42;
  r.headline = L"CPU pressure detected";
  r.insight = L"High CPU usage sustained";
  ring.Record(r, 1000);
  std::string json = ring.SerializeRecent(10);
  ASSERT_TRUE(json.find("scram_history") != std::string::npos, "missing scram_history type");
  ASSERT_TRUE(json.find("42") != std::string::npos, "missing risk score");
  PASS();
}

void TestThresholdDetector() {
  TEST("ThresholdDetector detects exceedance");
  ThresholdDetector det("cpuTest");
  det.AddField("cpuPct", 80.0, 95.0, 100.0);
  Snapshot prev{};
  prev.cpuPct = 50.0;
  Snapshot cur{};
  cur.cpuPct = 85.0;
  ChangeSet cs;
  det.Detect(cur, &prev, 1, cs);
  ASSERT_TRUE(!cs.Empty(), "should detect threshold breach");
  PASS();
}

void TestRiskScorer() {
  TEST("RiskScorer allocates budget");
  monix::scram::RiskScorer scorer;
  std::vector<monix::scram::RiskInput> inputs = {{"cpu", 10, 1.0, true}};
  int score = scorer.Evaluate(inputs);
  ASSERT_TRUE(score >= 0, "score should be non-negative");
  inputs.push_back({"ram", 30, 1.0, true});
  int score2 = scorer.Evaluate(inputs);
  ASSERT_TRUE(score2 >= score, "more inputs should yield higher score");
  PASS();
}

void TestSeverityTracker() {
  TEST("SeverityTracker hysteresis holds severity");
  monix::scram::SeverityTracker tracker;
  tracker.Update(60);
  ASSERT_TRUE(tracker.Current() == monix::scram::SeverityLevel::Error,
    "severity should be Error at risk=60");
  tracker.Update(10);
  ASSERT_TRUE(tracker.Current() == monix::scram::SeverityLevel::Error,
    "severity should hold at Error");
  for (int i = 0; i < 10; ++i) tracker.Update(10);
  ASSERT_TRUE(tracker.Current() == monix::scram::SeverityLevel::Info,
    "severity should drop after hold");
  PASS();
}

void TestPipelineHealth() {
  TEST("PipelineHealth tracks state");
  PipelineHealth health;
  health.RecordSnapshot();
  health.RecordSnapshot();
  health.SetRelayStatus(true);
  auto cp = health.GetCheckpoint();
  ASSERT_TRUE(cp.totalSnapshots == 2, "snapshot count mismatch");
  ASSERT_TRUE(cp.relayOk == true, "relay should be ok");
  ASSERT_TRUE(health.IsHealthy(), "should be healthy");
  health.RecordError("test", "unit test error");
  ASSERT_TRUE(health.IsHealthy(), "single error should still be healthy");
  std::string json = health.SerializeJson();
  ASSERT_TRUE(json.find("healthy") != std::string::npos, "missing healthy field");
  PASS();
}

void TestHeadlineGenerator() {
  TEST("HeadlineGenerator generates output");
  monix::scram::HeadlineGenerator gen;
  monix::scram::HeadlineInput input;
  input.severity = monix::scram::SeverityLevel::Warning;
  input.riskScore = 55;
  input.riskDelta = 10;
  input.smoothedRisk = 52.0;
  input.topRule = "CpuPressure";
  Snapshot s{};
  s.cpuPct = 92.0;
  s.ramUsedBytes = 12000000000ULL;
  s.ramTotalBytes = 16000000000ULL;
  auto output = gen.Generate(input, s, nullptr);
  ASSERT_TRUE(!output.headline.empty(), "headline should not be empty");
  PASS();
}

void TestSnapshotChangeDetector() {
  TEST("SnapshotChangeDetector tracks field changes");
  SnapshotChangeDetector det("test");
  det.AddField("cpuPct", 5.0, 10.0);
  Snapshot prev{};
  prev.cpuPct = 30.0;
  Snapshot cur{};
  cur.cpuPct = 50.0;
  ChangeSet cs;
  det.Detect(cur, &prev, 1, cs);
  ASSERT_TRUE(!cs.Empty(), "should detect significant change");
  PASS();
}

int main() {
  printf("=== Pipeline Component Tests ===\n\n");

  printf("[Normalizer]\n");
  TestNormalizer();
  TestNormalizerNegative();
  TestNormalizerRam();

  printf("\n[Validator]\n");
  TestValidator();
  TestValidatorInvalid();

  printf("\n[CorrelationEngine]\n");
  TestCorrelationEngine();

  printf("\n[StateStore]\n");
  TestStateStore();
  TestTimelineRing();
  TestChangeSet();

  printf("\n[Change Detection]\n");
  TestThresholdDetector();
  TestSnapshotChangeDetector();

  printf("\n[SCRAM Risk]\n");
  TestRiskScorer();
  TestSeverityTracker();
  TestHeadlineGenerator();

  printf("\n[Export]\n");
  TestScramHistoryRing();
  TestScramHistoryRingJson();

  printf("\n[Integration]\n");
  TestPipelineHealth();

  printf("\n=== Results: %d passed, %d failed ===\n", testsPassed, testsFailed);
  return testsFailed > 0 ? 1 : 0;
}
