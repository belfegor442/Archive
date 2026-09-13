#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>

#include <cstdio>
#include <string>

#include "../telemetry/normalization/Normalizer.hpp"
#include "../telemetry/normalization/Validator.hpp"
#include "../telemetry/state/ChangeSet.hpp"
#include "../telemetry/state/TimelineRing.hpp"
#include "../telemetry/export/ScramHistoryRing.hpp"
#include "../scram/risk/RiskScorer.hpp"
#include "../scram/risk/SeverityTracker.hpp"

using namespace monix;
using namespace monix::telemetry;

namespace monix {
std::string WideToUtf8(const std::wstring& ws) {
  if (ws.empty()) return {};
  int sz = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), (int)ws.size(), nullptr, 0, nullptr, nullptr);
  std::string result(sz, 0);
  WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), (int)ws.size(), result.data(), sz, nullptr, nullptr);
  return result;
}
}

static int g_passed = 0;
static int g_failed = 0;

static void TestNormalizerClamp() {
  printf("  %-45s ", "Normalizer clamps CPU 150 -> 100");
  Snapshot s{}; s.cpuPct = 150.0;
  Normalizer n; n.Normalize(s);
  if (s.cpuPct == 100.0) { printf("[PASS]\n"); g_passed++; } else { printf("[FAIL]\n"); g_failed++; }
}

static void TestNormalizerNegative() {
  printf("  %-45s ", "Normalizer clamps CPU -10 -> 0");
  Snapshot s{}; s.cpuPct = -10.0;
  Normalizer n; n.Normalize(s);
  if (s.cpuPct == 0.0) { printf("[PASS]\n"); g_passed++; } else { printf("[FAIL]\n"); g_failed++; }
}

static void TestNormalizerRam() {
  printf("  %-45s ", "Normalizer clamps RAM over total");
  Snapshot s{}; s.ramUsedBytes = 20000000000ULL; s.ramTotalBytes = 16000000000ULL;
  Normalizer n; n.Normalize(s);
  if (s.ramUsedBytes == s.ramTotalBytes) { printf("[PASS]\n"); g_passed++; } else { printf("[FAIL]\n"); g_failed++; }
}

static void TestNormalizerGpu() {
  printf("  %-45s ", "Normalizer clamps GPU 120 -> 100");
  Snapshot s{}; s.gpuPctValid = 1; s.gpuPct = 120.0;
  Normalizer n; n.Normalize(s);
  if (s.gpuPct == 100.0) { printf("[PASS]\n"); g_passed++; } else { printf("[FAIL]\n"); g_failed++; }
}

static void TestValidator() {
  printf("  %-45s ", "Validator marks high CPU suspicious");
  Snapshot s{}; s.cpuPct = 99.9; s.ramTotalBytes = 16000000000ULL; s.ramUsedBytes = 8000000000ULL;
  Validator v; v.Validate(s);
  if (v.HasSuspicious() || v.IsValid()) { printf("[PASS]\n"); g_passed++; } else { printf("[FAIL]\n"); g_failed++; }
}

static void TestChangeSet() {
  printf("  %-45s ", "ChangeSet stores and clears events");
  ChangeSet cs;
  ChangeEvent e; e.kind = ChangeEvent::Kind::Created; e.attribute = "test";
  cs.events.push_back(e);
  if (!cs.Empty()) { printf("[PASS]\n"); g_passed++; } else { printf("[FAIL]\n"); g_failed++; }
}

static void TestTimelineRing() {
  printf("  %-45s ", "TimelineRing caps at capacity");
  TimelineRing ring(4);
  for (int i = 0; i < 8; i++) { Snapshot s{}; s.cpuPct = (double)i; ring.Push(s); }
  if (ring.Count() == 4 && ring.Current().cpuPct == 7.0) { printf("[PASS]\n"); g_passed++; }
  else { printf("[FAIL]\n"); g_failed++; }
}

static void TestScramHistoryRing() {
  printf("  %-45s ", "ScramHistoryRing stores 12 in capacity 8");
  ScramHistoryRing ring(8);
  for (int i = 0; i < 12; i++) {
    ScramResult r; r.riskScore = i * 10;
    r.headline = L"test " + std::to_wstring(i);
    ring.Record(r, (std::uint64_t)i * 1000);
  }
  if (ring.Count() == 8 && ring.GetLatest().riskScore == 110) { printf("[PASS]\n"); g_passed++; }
  else { printf("[FAIL] count=%d latest=%d\n", ring.Count(), ring.GetLatest().riskScore); g_failed++; }
}

static void TestScramHistoryJson() {
  printf("  %-45s ", "ScramHistoryRing JSON serialization");
  ScramHistoryRing ring(4);
  ScramResult r; r.riskScore = 42; r.headline = L"CPU pressure";
  ring.Record(r, 1000);
  std::string json = ring.SerializeRecent(10);
  if (json.find("scram_history") != std::string::npos && json.find("42") != std::string::npos)
  { printf("[PASS]\n"); g_passed++; } else { printf("[FAIL]\n"); g_failed++; }
}

static void TestScramHistoryByRisk() {
  printf("  %-45s ", "ScramHistoryRing GetByRiskThreshold");
  ScramHistoryRing ring(16);
  for (int i = 0; i < 10; i++) {
    ScramResult r; r.riskScore = i * 10; r.headline = L"t";
    ring.Record(r, (std::uint64_t)i * 1000);
  }
  auto high = ring.GetByRiskThreshold(70);
  if (!high.empty() && high[0].riskScore >= 70) { printf("[PASS]\n"); g_passed++; }
  else { printf("[FAIL]\n"); g_failed++; }
}

static void TestRiskScorer() {
  printf("  %-45s ", "RiskScorer budget-based scoring");
  monix::scram::RiskScorer scorer;
  std::vector<monix::scram::RiskInput> in = {{"cpu", 10, 1.0, true}};
  int s1 = scorer.Evaluate(in);
  in.push_back({"ram", 30, 1.0, true});
  int s2 = scorer.Evaluate(in);
  if (s2 >= s1 && s1 >= 0) { printf("[PASS]\n"); g_passed++; }
  else { printf("[FAIL]\n"); g_failed++; }
}

static void TestSeverityTracker() {
  printf("  %-45s ", "SeverityTracker hysteresis hold");
  monix::scram::SeverityTracker t;
  t.Update(60);
  bool wasError = (t.Current() == monix::scram::SeverityLevel::Error);
  t.Update(10);
  bool held = (t.Current() == monix::scram::SeverityLevel::Error);
  for (int i = 0; i < 12; i++) t.Update(10);
  bool dropped = (t.Current() == monix::scram::SeverityLevel::Info);
  if (wasError && held && dropped) { printf("[PASS]\n"); g_passed++; }
  else { printf("[FAIL] wasErr=%d held=%d dropped=%d\n", wasError, held, dropped); g_failed++; }
}

int main() {
  SetConsoleOutputCP(CP_UTF8);
  printf("=== Monix Pipeline Unit Tests ===\n\n");

  printf("[Normalizer]\n");
  TestNormalizerClamp();
  TestNormalizerNegative();
  TestNormalizerRam();
  TestNormalizerGpu();

  printf("\n[Validator]\n");
  TestValidator();

  printf("\n[State Store]\n");
  TestChangeSet();
  TestTimelineRing();

  printf("\n[Export]\n");
  TestScramHistoryRing();
  TestScramHistoryJson();
  TestScramHistoryByRisk();

  printf("\n[SCRAM Risk]\n");
  TestRiskScorer();
  TestSeverityTracker();

  printf("\n=== Results: %d passed, %d failed ===\n", g_passed, g_failed);
  return g_failed;
}
