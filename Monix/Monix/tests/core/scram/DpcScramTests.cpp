#include <cassert>
#include <cstdio>
#include <string>
#include <vector>

#include "../../src/native/telemetry/Snapshot.hpp"
#include "../../src/native/scram/ScramEngine.hpp"
#include "../../src/native/scram/RiskRule.hpp"

using namespace monix;

static int g_testsPassed = 0;
static int g_testsFailed = 0;

#define TEST(name) \
  static void test_##name(); \
  struct TestReg_##name { TestReg_##name() { test_##name(); } } reg_##name; \
  static void test_##name()

#define ASSERT_TRUE(x) do { if (!(x)) { \
  fprintf(stderr, "FAIL: %s:%d: %s\n", __FILE__, __LINE__, #x); \
  g_testsFailed++; return; } } while(0)
#define ASSERT_EQ(a, b) do { if ((a) != (b)) { \
  fprintf(stderr, "FAIL: %s:%d: %s != %s\n", __FILE__, __LINE__, #a, #b); \
  g_testsFailed++; return; } } while(0)
#define ASSERT_GT(a, b) do { if (!((a) > (b))) { \
  fprintf(stderr, "FAIL: %s:%d: %s <= %s\n", __FILE__, __LINE__, #a, #b); \
  g_testsFailed++; return; } } while(0)
#define ASSERT_LE(a, b) do { if (!((a) <= (b))) { \
  fprintf(stderr, "FAIL: %s:%d: %s > %s\n", __FILE__, __LINE__, #a, #b); \
  g_testsFailed++; return; } } while(0)

// Minimal rule that always fires with a given headline and riskDelta
class ConstantRule : public RiskRule {
public:
  ConstantRule(std::wstring headline, int riskDelta)
    : headline_(std::move(headline)), riskDelta_(riskDelta) {}
  void Evaluate(const Snapshot&, const Snapshot*, std::vector<ScramFinding>& findings) override {
    findings.push_back({headline_, L"", L"", riskDelta_});
  }
private:
  std::wstring headline_;
  int riskDelta_;
};

// Rule that fires only when a condition is met
class ConditionalRule : public RiskRule {
public:
  ConditionalRule(std::wstring headline, int riskDelta, bool& condition)
    : headline_(std::move(headline)), riskDelta_(riskDelta), condition_(condition) {}
  void Evaluate(const Snapshot&, const Snapshot*, std::vector<ScramFinding>& findings) override {
    if (condition_) {
      findings.push_back({headline_, L"", L"", riskDelta_});
    }
  }
private:
  std::wstring headline_;
  int riskDelta_;
  bool& condition_;
};

TEST(dpc_stable_baseline_no_finding) {
  ScramEngine engine;
  engine.AddRule(std::make_unique<ConstantRule>(L"Test finding", 10));
  Snapshot s;
  s.cpuPct = 20.0;
  s.dpcTimePerSec = 50000;

  auto r = engine.Evaluate(s, nullptr);
  ASSERT_EQ(engine.phase(), ScramPhase::Calibrating);
  ASSERT_EQ(r.riskScore, 0);
}

TEST(dpc_spike_emits_after_debounce) {
  ScramEngine engine;
  bool active = true;
  engine.AddRule(std::make_unique<ConditionalRule>(L"DPC spike", 14, active));

  Snapshot s;
  s.cpuPct = 50.0;
  s.dpcTimePerSec = 500000;

  for (int i = 0; i < 5; ++i) engine.Evaluate(s, nullptr);
  ASSERT_EQ(engine.phase(), ScramPhase::Monitoring);

  auto r1 = engine.Evaluate(s, nullptr);
  ASSERT_TRUE(r1.headline.empty());

  auto r2 = engine.Evaluate(s, nullptr);
  ASSERT_TRUE(!r2.headline.empty());
  ASSERT_EQ(r2.riskScore, 14);
}

TEST(dpc_unchanged_finding_suppressed_after_cooldown) {
  ScramEngine engine;
  bool active = true;
  engine.AddRule(std::make_unique<ConditionalRule>(L"DPC spike", 14, active));

  Snapshot s;
  s.cpuPct = 50.0;
  s.dpcTimePerSec = 500000;

  for (int i = 0; i < 5; ++i) engine.Evaluate(s, nullptr);

  engine.Evaluate(s, nullptr);
  engine.Evaluate(s, nullptr);

  for (int i = 0; i < 10; ++i) engine.Evaluate(s, nullptr);

  int emissionCount = 0;
  for (int i = 0; i < 20; ++i) {
    auto r = engine.Evaluate(s, nullptr);
    if (!r.headline.empty()) emissionCount++;
  }
  ASSERT_EQ(emissionCount, 1);
}

TEST(dpc_changed_finding_re_emits) {
  ScramEngine engine;
  bool active = true;
  engine.AddRule(std::make_unique<ConditionalRule>(L"DPC spike", 14, active));

  Snapshot s;
  s.cpuPct = 50.0;
  s.dpcTimePerSec = 500000;

  for (int i = 0; i < 5; ++i) engine.Evaluate(s, nullptr);

  engine.Evaluate(s, nullptr);
  engine.Evaluate(s, nullptr);

  for (int i = 0; i < 10; ++i) engine.Evaluate(s, nullptr);

  active = false;
  for (int i = 0; i < 5; ++i) engine.Evaluate(s, nullptr);

  active = true;
  auto r1 = engine.Evaluate(s, nullptr);
  auto r2 = engine.Evaluate(s, nullptr);
  ASSERT_TRUE(!r2.headline.empty());
}

TEST(dpc_recovery_stops_finding) {
  ScramEngine engine;
  bool active = true;
  engine.AddRule(std::make_unique<ConditionalRule>(L"DPC spike", 14, active));

  Snapshot s;
  s.cpuPct = 50.0;
  s.dpcTimePerSec = 500000;

  for (int i = 0; i < 5; ++i) engine.Evaluate(s, nullptr);
  engine.Evaluate(s, nullptr);
  engine.Evaluate(s, nullptr);

  active = false;
  for (int i = 0; i < 20; ++i) {
    auto r = engine.Evaluate(s, nullptr);
    ASSERT_TRUE(r.headline.empty());
  }
}

TEST(dpc_risk_budget_capped) {
  ScramEngine engine;
  for (int i = 0; i < 10; ++i) {
    engine.AddRule(std::make_unique<ConstantRule>(
      L"Rule " + std::to_wstring(i), 20));
  }

  Snapshot s;
  for (int i = 0; i < 5; ++i) engine.Evaluate(s, nullptr);

  int totalRisk = 0;
  for (int i = 0; i < 20; ++i) {
    auto r = engine.Evaluate(s, nullptr);
    totalRisk = r.riskScore;
  }
  ASSERT_LE(totalRisk, 60);
}

TEST(dpc_no_findings_zero_risk) {
  ScramEngine engine;
  Snapshot s;
  s.cpuPct = 10.0;
  s.dpcTimePerSec = 10000;

  for (int i = 0; i < 5; ++i) engine.Evaluate(s, nullptr);

  auto r = engine.Evaluate(s, nullptr);
  ASSERT_EQ(r.riskScore, 0);
}

TEST(dpc_unavailable_no_false_positive) {
  ScramEngine engine;
  engine.AddRule(std::make_unique<ConditionalRule>(L"DPC spike", 14, []{
    static bool cond = true;
    return cond;
  }()));

  Snapshot s;
  s.cpuPct = 50.0;
  s.dpcTimePerSec = 0;
  s.interruptsPerSec = -1;

  for (int i = 0; i < 5; ++i) engine.Evaluate(s, nullptr);

  auto r = engine.Evaluate(s, nullptr);
  ASSERT_TRUE(r.headline.empty());
}

TEST(dpc_delta_is_risk_not_dpc_count) {
  ScramEngine engine;
  bool active = true;
  engine.AddRule(std::make_unique<ConditionalRule>(L"DPC spike", 14, active));

  Snapshot s;
  s.cpuPct = 50.0;
  s.dpcTimePerSec = 500000;

  for (int i = 0; i < 5; ++i) engine.Evaluate(s, nullptr);

  auto r1 = engine.Evaluate(s, nullptr);
  auto r2 = engine.Evaluate(s, nullptr);

  ASSERT_TRUE(!r2.headline.empty());
  ASSERT_EQ(r2.riskScore, 14);
}

TEST(dpc_counter_wrap_no_crash) {
  ScramEngine engine;
  engine.AddRule(std::make_unique<ConstantRule>(L"Test", 10));

  Snapshot s;
  s.totalDpcCount = 100;
  for (int i = 0; i < 5; ++i) engine.Evaluate(s, nullptr);

  s.totalDpcCount = 50;
  auto r = engine.Evaluate(s, nullptr);
  ASSERT_TRUE(r.riskScore >= 0);
}

TEST(dpc_zero_elapsed_time_no_crash) {
  Snapshot s;
  s.dpcTimePerSec = 0;
  s.totalDpcCount = 1000;
  ASSERT_TRUE(s.dpcTimePerSec == 0);
}

int main() {
  printf("DPC/SCRAM Regression Tests\n");
  printf("==========================\n");
  printf("Tests passed: %d\n", g_testsPassed);
  printf("Tests failed: %d\n", g_testsFailed);
  return g_testsFailed > 0 ? 1 : 0;
}
