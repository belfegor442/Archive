#include <cassert>
#include <chrono>
#include <iostream>
#include <string>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "CircuitBreaker.hpp"
#include "EventCorrelation.hpp"
#include "ActivityModel.hpp"
#include "NoiseControl.hpp"

using namespace monix::collectors::circuit;
using namespace monix::collectors::correlation;
using namespace monix::collectors::activity;
using namespace monix::collectors::noise;

struct TestResult { std::string name; bool passed; };
static std::vector<TestResult> results;

#define RUN_TEST(name) do { \
  std::cerr << "  " << #name << "... "; \
  bool ok = false; \
  try { ok = test_##name(); } catch (...) { ok = false; } \
  results.push_back({#name, ok}); \
  if (ok) std::cerr << "[PASS]\n"; else std::cerr << "[FAIL]\n"; \
} while(0)

#define ASSERT_TRUE(expr) do { if (!(expr)) { std::cerr << "\n    FAIL: " << #expr << " (line " << __LINE__ << ")"; return false; } } while(0)
#define ASSERT_FALSE(expr) ASSERT_TRUE(!(expr))
#define ASSERT_EQ(a, b) do { if ((a) != (b)) { std::cerr << "\n    FAIL: " << #a << " != " << #b << " (line " << __LINE__ << ")"; return false; } } while(0)

static std::int64_t msNow() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::system_clock::now().time_since_epoch()).count();
}

// ===== Circuit Breaker Tests =====

static bool test_cb_state_names() {
  ASSERT_EQ(std::string(CircuitStateName(CircuitState::Closed)), "Closed");
  ASSERT_EQ(std::string(CircuitStateName(CircuitState::Open)), "Open");
  ASSERT_EQ(std::string(CircuitStateName(CircuitState::HalfOpen)), "HalfOpen");
  return true;
}

static bool test_cb_event_actions() {
  ASSERT_EQ(CircuitEventKindAction(CircuitEventKind::Opened), "collector.circuit_open");
  ASSERT_EQ(CircuitEventKindAction(CircuitEventKind::Closed), "collector.circuit_closed");
  ASSERT_EQ(CircuitEventKindAction(CircuitEventKind::Rejected), "collector.circuit_rejected");
  return true;
}

static bool test_cb_threshold() {
  CircuitBreakerConfig cfg;
  cfg.failure_threshold = 3;
  cfg.cooldown_ms = 50;
  cfg.enabled = true;
  CircuitBreaker cb(cfg);

  ASSERT_TRUE(cb.allowRequest("P"));
  cb.recordFailure("P");
  ASSERT_TRUE(cb.allowRequest("P"));
  cb.recordFailure("P");
  ASSERT_TRUE(cb.allowRequest("P"));
  cb.recordFailure("P");
  ASSERT_FALSE(cb.allowRequest("P"));
  ASSERT_TRUE(cb.isOpen("P"));
  return true;
}

static bool test_cb_open() {
  CircuitBreakerConfig cfg;
  cfg.failure_threshold = 2;
  cfg.cooldown_ms = 50;
  CircuitBreaker cb(cfg);
  cb.setCallback([](const CircuitEvent&) {});

  cb.recordFailure("X");
  cb.recordFailure("X");
  ASSERT_TRUE(cb.isOpen("X"));
  ASSERT_FALSE(cb.allowRequest("X"));
  return true;
}

static bool test_cb_cooldown() {
  CircuitBreakerConfig cfg;
  cfg.failure_threshold = 1;
  cfg.cooldown_ms = 50;
  cfg.half_open_max_calls = 1;
  CircuitBreaker cb(cfg);
  cb.setCallback([](const CircuitEvent&) {});

  cb.recordFailure("Y");
  ASSERT_TRUE(cb.isOpen("Y"));
  ASSERT_FALSE(cb.allowRequest("Y"));

  Sleep(80);

  ASSERT_TRUE(cb.allowRequest("Y"));
  ASSERT_TRUE(cb.isHalfOpen("Y"));
  return true;
}

static bool test_cb_half_open_recovery() {
  CircuitBreakerConfig cfg;
  cfg.failure_threshold = 1;
  cfg.cooldown_ms = 30;
  cfg.half_open_max_calls = 1;
  CircuitBreaker cb(cfg);
  cb.setCallback([](const CircuitEvent&) {});

  cb.recordFailure("Z");
  Sleep(50);
  cb.allowRequest("Z");
  cb.recordSuccess("Z");
  ASSERT_TRUE(cb.getState("Z") == CircuitState::Closed);
  ASSERT_FALSE(cb.isOpen("Z"));
  return true;
}

static bool test_cb_disabled() {
  CircuitBreakerConfig cfg;
  cfg.enabled = false;
  cfg.failure_threshold = 1;
  CircuitBreaker cb(cfg);

  for (int i = 0; i < 100; i++) cb.recordFailure("D");
  ASSERT_TRUE(cb.allowRequest("D"));
  return true;
}

static bool test_cb_reset() {
  CircuitBreakerConfig cfg;
  cfg.failure_threshold = 2;
  CircuitBreaker cb(cfg);

  cb.recordFailure("R");
  cb.recordFailure("R");
  ASSERT_TRUE(cb.isOpen("R"));
  cb.reset("R");
  ASSERT_FALSE(cb.isOpen("R"));
  return true;
}

// ===== Event Correlation Tests =====

static bool test_corr_simple_chain() {
  CorrelationEngine engine;
  CorrelatedEvent e1{"E1", "", "C1", "A1", "S1", "", "login", "user", 1000};
  CorrelatedEvent e2{"E2", "E1", "C1", "A1", "S1", "", "process", "system", 2000};
  CorrelatedEvent e3{"E3", "E2", "C1", "A1", "S1", "", "script", "system", 3000};
  engine.addEvent(e1);
  engine.addEvent(e2);
  engine.addEvent(e3);

  auto chain = engine.buildChain("C1");
  ASSERT_TRUE(chain.isValid());
  ASSERT_EQ(chain.size(), static_cast<std::size_t>(3));
  return true;
}

static bool test_corr_nested_chain() {
  CorrelationEngine engine;
  CorrelatedEvent e1{"E1", "", "C1", "", "", "", "outer", "s", 1000};
  CorrelatedEvent e2{"E2", "E1", "C1", "", "", "", "inner1", "s", 2000};
  CorrelatedEvent e3{"E3", "E2", "C1", "", "", "", "inner2", "s", 3000};
  engine.addEvent(e1);
  engine.addEvent(e2);
  engine.addEvent(e3);

  auto children = engine.getChildren("E1");
  ASSERT_EQ(children.size(), static_cast<std::size_t>(1));
  ASSERT_EQ(children[0].event_id, "E2");

  children = engine.getChildren("E2");
  ASSERT_EQ(children.size(), static_cast<std::size_t>(1));
  ASSERT_EQ(children[0].event_id, "E3");
  return true;
}

static bool test_corr_parallel_activity() {
  CorrelationEngine engine;
  CorrelatedEvent e1{"E1", "", "", "A1", "", "", "a", "s", 1000};
  CorrelatedEvent e2{"E2", "", "", "A1", "", "", "b", "s", 2000};
  CorrelatedEvent e3{"E3", "", "", "A1", "", "", "c", "s", 3000};
  engine.addEvent(e1);
  engine.addEvent(e2);
  engine.addEvent(e3);

  auto events = engine.byActivity("A1");
  ASSERT_EQ(events.size(), static_cast<std::size_t>(3));
  ASSERT_EQ(engine.activityCount(), static_cast<std::size_t>(1));
  return true;
}

static bool test_corr_same_process() {
  CorrelationEngine engine;
  CorrelatedEvent e1{"E1", "", "C1", "", "", "R1", "start", "proc", 1000};
  CorrelatedEvent e2{"E2", "", "C1", "", "", "R1", "read", "proc", 2000};
  CorrelatedEvent e3{"E3", "", "C1", "", "", "R1", "write", "proc", 3000};
  engine.addEvent(e1);
  engine.addEvent(e2);
  engine.addEvent(e3);

  auto events = engine.byRequest("R1");
  ASSERT_EQ(events.size(), static_cast<std::size_t>(3));
  return true;
}

static bool test_corr_session() {
  CorrelationEngine engine;
  CorrelatedEvent e1{"E1", "", "", "", "S1", "", "login", "user", 1000};
  CorrelatedEvent e2{"E2", "", "", "", "S1", "", "action", "user", 2000};
  engine.addEvent(e1);
  engine.addEvent(e2);

  auto events = engine.bySession("S1");
  ASSERT_EQ(events.size(), static_cast<std::size_t>(2));
  return true;
}

static bool test_corr_pid_reuse() {
  CorrelationEngine engine;
  CorrelatedEvent e1{"E1", "", "C1", "", "", "", "proc1_start", "p1", 1000};
  CorrelatedEvent e2{"E2", "", "C2", "", "", "", "proc2_start", "p1", 2000};
  engine.addEvent(e1);
  engine.addEvent(e2);

  auto c1 = engine.byCorrelationId("C1");
  auto c2 = engine.byCorrelationId("C2");
  ASSERT_EQ(c1.size(), static_cast<std::size_t>(1));
  ASSERT_EQ(c2.size(), static_cast<std::size_t>(1));
  ASSERT_EQ(engine.chainCount(), static_cast<std::size_t>(2));
  return true;
}

static bool test_corr_remove_event() {
  CorrelationEngine engine;
  CorrelatedEvent e1{"E1", "", "C1", "A1", "S1", "R1", "t", "s", 1000};
  engine.addEvent(e1);
  ASSERT_TRUE(engine.hasEvent("E1"));
  engine.removeEvent("E1");
  ASSERT_FALSE(engine.hasEvent("E1"));
  ASSERT_EQ(engine.byCorrelationId("C1").size(), static_cast<std::size_t>(0));
  return true;
}

// ===== Activity Model Tests =====

static bool test_act_create() {
  ActivityTracker tracker;
  auto id = tracker.createActivity("TestActivity");
  ASSERT_TRUE(!id.empty());
  ASSERT_TRUE(tracker.isActive(id));
  ASSERT_EQ(tracker.activeCount(), static_cast<std::size_t>(1));
  return true;
}

static bool test_act_add_event() {
  ActivityTracker tracker;
  auto id = tracker.createActivity("Test");
  ActivityEventRecord ev{"EV1", "file.modified", "fs", "test file", 1000};
  ASSERT_TRUE(tracker.addEvent(id, ev));

  ActivityRecord act = tracker.getActivity(id);
  ASSERT_EQ(act.eventCount(), static_cast<std::size_t>(1));
  ASSERT_TRUE(act.isActive());
  return true;
}

static bool test_act_complete() {
  ActivityTracker tracker;
  auto id = tracker.createActivity("Done");
  tracker.addEvent(id, {"EV1", "t", "s", "d", 1000});
  ASSERT_TRUE(tracker.completeActivity(id));

  ActivityRecord act = tracker.getActivity(id);
  ASSERT_TRUE(act.isComplete());
  ASSERT_FALSE(act.isActive());
  return true;
}

static bool test_act_fail() {
  ActivityTracker tracker;
  auto id = tracker.createActivity("Fail");
  ASSERT_TRUE(tracker.failActivity(id, "error"));
  ActivityRecord act = tracker.getActivity(id);
  ASSERT_TRUE(act.isComplete());
  return true;
}

static bool test_act_multiple_events() {
  ActivityTracker tracker;
  auto id = tracker.createActivity("Multi");
  for (int i = 0; i < 10; i++) {
    ActivityEventRecord ev{"EV" + std::to_string(i), "type", "src", "d", 1000 + i};
    tracker.addEvent(id, ev);
  }
  ActivityRecord act = tracker.getActivity(id);
  ASSERT_EQ(act.eventCount(), static_cast<std::size_t>(10));
  return true;
}

static bool test_act_lifecycle_events() {
  ActivityTracker tracker;
  std::size_t event_count = 0;
  tracker.setCallback([&](const ActivityLifecycleEvent&) { event_count++; });

  auto id = tracker.createActivity("Lifecycle");
  tracker.addEvent(id, {"EV1", "t", "s", "d", 1000});
  tracker.completeActivity(id);
  ASSERT_TRUE(event_count >= static_cast<std::size_t>(3));
  return true;
}

static bool test_act_cannot_add_after_complete() {
  ActivityTracker tracker;
  auto id = tracker.createActivity("Closed");
  tracker.completeActivity(id);
  ASSERT_FALSE(tracker.addEvent(id, {"EV1", "t", "s", "d", 1000}));
  return true;
}

// ===== Noise Control Tests =====

static bool test_nc_mechanism_names() {
  ASSERT_EQ(std::string(NoiseMechanismName(NoiseMechanism::Deduplication)), "Deduplication");
  ASSERT_EQ(std::string(NoiseMechanismName(NoiseMechanism::RateLimiting)), "RateLimiting");
  ASSERT_EQ(std::string(NoiseMechanismName(NoiseMechanism::Threshold)), "Threshold");
  return true;
}

static bool test_nc_verdict_names() {
  ASSERT_EQ(std::string(NoiseVerdictName(NoiseVerdict::Pass)), "Pass");
  ASSERT_EQ(std::string(NoiseVerdictName(NoiseVerdict::Deduplicate)), "Deduplicate");
  ASSERT_EQ(std::string(NoiseVerdictName(NoiseVerdict::RateLimited)), "RateLimited");
  return true;
}

static bool test_nc_dedup() {
  NoiseControlEngine engine;
  CollectorNoisePolicy policy;
  policy.collector_name = "net";
  policy.mechanisms = NoiseMechanism::Deduplication;
  policy.dedup_window_ms = 1000;
  engine.setPolicy(policy);

  NoiseEvent e1{"EV1", "net", "conn", 1000, 0};
  NoiseEvent e2{"EV1", "net", "conn", 1500, 0};
  NoiseEvent e3{"EV2", "net", "conn", 2000, 0};

  ASSERT_TRUE(engine.evaluate(e1).passes());
  ASSERT_FALSE(engine.evaluate(e2).passes());
  ASSERT_TRUE(engine.evaluate(e3).passes());
  return true;
}

static bool test_nc_rate_limit() {
  NoiseControlEngine engine;
  CollectorNoisePolicy policy;
  policy.collector_name = "fs";
  policy.mechanisms = NoiseMechanism::RateLimiting;
  policy.rate_limit_max = 3;
  policy.rate_limit_window_ms = 1000;
  engine.setPolicy(policy);

  ASSERT_TRUE(engine.evaluate({"E1", "fs", "mod", 1000, 0}).passes());
  ASSERT_TRUE(engine.evaluate({"E2", "fs", "mod", 1000, 0}).passes());
  ASSERT_TRUE(engine.evaluate({"E3", "fs", "mod", 1000, 0}).passes());
  ASSERT_FALSE(engine.evaluate({"E4", "fs", "mod", 1000, 0}).passes());
  return true;
}

static bool test_nc_threshold() {
  NoiseControlEngine engine;
  CollectorNoisePolicy policy;
  policy.collector_name = "sensor";
  policy.mechanisms = NoiseMechanism::Threshold;
  policy.threshold_min = 50.0;
  engine.setPolicy(policy);

  ASSERT_FALSE(engine.evaluate({"E1", "sensor", "temp", 1000, 30.0}).passes());
  ASSERT_TRUE(engine.evaluate({"E2", "sensor", "temp", 1000, 60.0}).passes());
  ASSERT_TRUE(engine.evaluate({"E3", "sensor", "temp", 1000, 50.0}).passes());
  return true;
}

static bool test_nc_no_policy() {
  NoiseControlEngine engine;
  NoiseEvent e{"E1", "unknown", "t", 1000, 0};
  ASSERT_TRUE(engine.evaluate(e).passes());
  return true;
}

static bool test_nc_stats() {
  NoiseControlEngine engine;
  CollectorNoisePolicy policy;
  policy.collector_name = "net";
  policy.mechanisms = NoiseMechanism::Deduplication;
  policy.dedup_window_ms = 1000;
  engine.setPolicy(policy);

  engine.evaluate({"E1", "net", "t", 1000, 0});
  engine.evaluate({"E1", "net", "t", 1100, 0});
  engine.evaluate({"E2", "net", "t", 1200, 0});

  ASSERT_EQ(engine.totalEvents(), static_cast<std::size_t>(3));
  ASSERT_EQ(engine.totalFiltered(), static_cast<std::size_t>(1));
  ASSERT_EQ(engine.totalPassed(), static_cast<std::size_t>(2));
  return true;
}

static bool test_nc_mechanism_flags() {
  NoiseMechanism m = NoiseMechanism::Deduplication | NoiseMechanism::RateLimiting;
  ASSERT_TRUE(hasMechanism(m, NoiseMechanism::Deduplication));
  ASSERT_TRUE(hasMechanism(m, NoiseMechanism::RateLimiting));
  ASSERT_FALSE(hasMechanism(m, NoiseMechanism::Threshold));
  return true;
}

#ifndef MONIX_KERNEL_BUILD
int main() {
  std::cerr << "=== MONIX Phases 23-26 Tests ===\n\n";

  RUN_TEST(cb_state_names);
  RUN_TEST(cb_event_actions);
  RUN_TEST(cb_threshold);
  RUN_TEST(cb_open);
  RUN_TEST(cb_cooldown);
  RUN_TEST(cb_half_open_recovery);
  RUN_TEST(cb_disabled);
  RUN_TEST(cb_reset);
  RUN_TEST(corr_simple_chain);
  RUN_TEST(corr_nested_chain);
  RUN_TEST(corr_parallel_activity);
  RUN_TEST(corr_same_process);
  RUN_TEST(corr_session);
  RUN_TEST(corr_pid_reuse);
  RUN_TEST(corr_remove_event);
  RUN_TEST(act_create);
  RUN_TEST(act_add_event);
  RUN_TEST(act_complete);
  RUN_TEST(act_fail);
  RUN_TEST(act_multiple_events);
  RUN_TEST(act_lifecycle_events);
  RUN_TEST(act_cannot_add_after_complete);
  RUN_TEST(nc_mechanism_names);
  RUN_TEST(nc_verdict_names);
  RUN_TEST(nc_dedup);
  RUN_TEST(nc_rate_limit);
  RUN_TEST(nc_threshold);
  RUN_TEST(nc_no_policy);
  RUN_TEST(nc_stats);
  RUN_TEST(nc_mechanism_flags);

  int passed = 0, failed = 0;
  for (const auto& r : results) { if (r.passed) passed++; else failed++; }
  std::cerr << "\n=== Results: " << passed << " passed, " << failed << " failed ===\n";
  return failed > 0 ? 1 : 0;
}
#endif

#ifdef MONIX_KERNEL_BUILD

struct KBoolTestEntry {
  const char* display_name;
  bool (*func)();
};

static const KBoolTestEntry s_kbooltests[] = {
  {"CB state names", test_cb_state_names},
  {"CB event actions", test_cb_event_actions},
  {"CB threshold", test_cb_threshold},
  {"CB open", test_cb_open},
  {"CB cooldown", test_cb_cooldown},
  {"CB half-open recovery", test_cb_half_open_recovery},
  {"CB disabled", test_cb_disabled},
  {"CB reset", test_cb_reset},
  {"Correlation simple chain", test_corr_simple_chain},
  {"Correlation nested chain", test_corr_nested_chain},
  {"Correlation parallel activity", test_corr_parallel_activity},
  {"Correlation same process", test_corr_same_process},
  {"Correlation session", test_corr_session},
  {"Correlation PID reuse", test_corr_pid_reuse},
  {"Correlation remove event", test_corr_remove_event},
  {"Activity create", test_act_create},
  {"Activity add event", test_act_add_event},
  {"Activity complete", test_act_complete},
  {"Activity fail", test_act_fail},
  {"Activity multiple events", test_act_multiple_events},
  {"Activity lifecycle events", test_act_lifecycle_events},
  {"Activity cannot add after complete", test_act_cannot_add_after_complete},
  {"Notification mechanism names", test_nc_mechanism_names},
  {"Notification verdict names", test_nc_verdict_names},
  {"Notification dedup", test_nc_dedup},
  {"Notification rate limit", test_nc_rate_limit},
  {"Notification threshold", test_nc_threshold},
  {"Notification no policy", test_nc_no_policy},
  {"Notification stats", test_nc_stats},
  {"Notification mechanism flags", test_nc_mechanism_flags},
};

const KBoolTestEntry* GetKBoolTests_Phases23_26() { return s_kbooltests; }
std::size_t GetKBoolTestCount_Phases23_26() { return sizeof(s_kbooltests) / sizeof(s_kbooltests[0]); }
#endif
