#include <cassert>
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

#include "ObservationGap.hpp"
#include "CollectorHealth.hpp"
#include "RestartPolicy.hpp"

using namespace monix::collectors::health;

struct TestResult {
  std::string name;
  bool passed;
};

static std::vector<TestResult> results;

#define RUN_TEST(name) do { \
  std::cerr << "  " << #name << "... "; \
  bool ok = false; \
  try { ok = test_##name(); } catch (...) { ok = false; } \
  results.push_back({#name, ok}); \
  if (ok) std::cerr << "[PASS]\n"; else std::cerr << "[FAIL]\n"; \
} while(0)

#define ASSERT_TRUE(expr) do { \
  if (!(expr)) { \
    std::cerr << "\n    FAIL: " << #expr << " (line " << __LINE__ << ")"; \
    return false; \
  } \
} while(0)

#define ASSERT_FALSE(expr) ASSERT_TRUE(!(expr))
#define ASSERT_EQ(a, b) do { if ((a) != (b)) { std::cerr << "\n    FAIL: " << #a << " != " << #b << " (line " << __LINE__ << ")"; return false; } } while(0)

static bool test_gap_event_names() {
  ASSERT_EQ(std::string(GapEventKindName(GapEventKind::GapDetected)), "GapDetected");
  ASSERT_EQ(std::string(GapEventKindName(GapEventKind::Recovered)), "Recovered");
  ASSERT_EQ(GapEventKindAction(GapEventKind::GapDetected), "collector.observation_gap");
  ASSERT_EQ(GapEventKindAction(GapEventKind::Recovered), "collector.recovered");
  return true;
}

static bool test_health_state_names() {
  ASSERT_EQ(std::string(CollectorHealthStateName(CollectorHealthState::Running)), "Running");
  ASSERT_EQ(std::string(CollectorHealthStateName(CollectorHealthState::Stopped)), "Stopped");
  ASSERT_EQ(std::string(CollectorHealthStateName(CollectorHealthState::Failed)), "Failed");
  ASSERT_EQ(std::string(CollectorHealthStateName(CollectorHealthState::Degraded)), "Degraded");
  return true;
}

static bool test_gap_validity() {
  ObservationGap gap;
  ASSERT_FALSE(gap.isValid());
  gap.collector = "ProcessCollector";
  ASSERT_TRUE(gap.isValid());
  ASSERT_TRUE(gap.isActive());
  gap.ended_at_ms = 100;
  ASSERT_FALSE(gap.isActive());
  return true;
}

static bool test_health_event_action() {
  HealthEvent e;
  e.collector = "Test";
  e.state = CollectorHealthState::Started;
  ASSERT_EQ(e.action(), "collector.started");
  e.state = CollectorHealthState::Stopped;
  ASSERT_EQ(e.action(), "collector.stopped");
  e.state = CollectorHealthState::Degraded;
  ASSERT_EQ(e.action(), "collector.degraded");
  e.state = CollectorHealthState::Failed;
  ASSERT_EQ(e.action(), "collector.failed");
  e.state = CollectorHealthState::Recovered;
  ASSERT_EQ(e.action(), "collector.recovered");
  return true;
}

static bool test_metrics_validity() {
  CollectorMetrics m;
  ASSERT_TRUE(m.isValid());
  ASSERT_EQ(m.errorRate(), 0.0);
  ASSERT_EQ(m.dropRate(), 0.0);
  m.events_generated = 100;
  m.errors = 10;
  ASSERT_TRUE(m.errorRate() > 0.09 && m.errorRate() < 0.11);
  m.events_dropped = 5;
  ASSERT_TRUE(m.dropRate() > 0.04 && m.dropRate() < 0.06);
  return true;
}

static bool test_metrics_no_division_by_zero() {
  CollectorMetrics m;
  m.events_generated = 0;
  ASSERT_EQ(m.errorRate(), 0.0);
  ASSERT_EQ(m.dropRate(), 0.0);
  return true;
}

static bool test_gap_detector_lifecycle() {
  ObservationGapDetector detector;
  detector.setGapCallback([](const GapEvent&) {});
  detector.setHealthCallback([](const HealthEvent&) {});

  detector.collectorStarted("Process", 1000);
  ASSERT_TRUE(detector.isRunning("Process"));
  ASSERT_FALSE(detector.hasGap("Process"));

  detector.collectorStopped("Process", "crash", 2000);
  ASSERT_FALSE(detector.isRunning("Process"));

  detector.collectorStarted("Process", 5000);
  ASSERT_TRUE(detector.isRunning("Process"));
  ASSERT_TRUE(detector.hasGap("Process"));
  ASSERT_EQ(detector.gapCount("Process"), static_cast<std::size_t>(1));
  return true;
}

static bool test_gap_duration() {
  ObservationGapDetector detector;
  detector.setGapCallback([](const GapEvent&) {});
  detector.setHealthCallback([](const HealthEvent&) {});

  detector.collectorStarted("A", 1000);
  detector.collectorStopped("A", "error", 2000);
  detector.collectorStarted("A", 5000);

  ObservationGap gap = detector.lastGap("A");
  ASSERT_TRUE(gap.isValid());
  ASSERT_EQ(gap.duration_ms, 3000);
  return true;
}

static bool test_multiple_gaps() {
  ObservationGapDetector detector;
  detector.setGapCallback([](const GapEvent&) {});
  detector.setHealthCallback([](const HealthEvent&) {});

  for (int i = 0; i < 5; i++) {
    std::int64_t start = 1000 + i * 10000;
    detector.collectorStarted("B", start);
    detector.collectorStopped("B", "test", start + 1000);
  }
  detector.collectorStarted("B", 51000);

  ASSERT_EQ(detector.gapCount("B"), static_cast<std::size_t>(5));
  ASSERT_EQ(detector.totalGaps(), static_cast<std::size_t>(5));
  return true;
}

static bool test_short_gap() {
  ObservationGapDetector detector;
  detector.setGapCallback([](const GapEvent&) {});
  detector.setHealthCallback([](const HealthEvent&) {});

  detector.collectorStarted("C", 1000);
  detector.collectorStopped("C", "brief", 1050);
  detector.collectorStarted("C", 1100);

  ObservationGap gap = detector.lastGap("C");
  ASSERT_EQ(gap.duration_ms, 50);
  return true;
}

static bool test_long_gap() {
  ObservationGapDetector detector;
  detector.setGapCallback([](const GapEvent&) {});
  detector.setHealthCallback([](const HealthEvent&) {});

  detector.collectorStarted("D", 1000);
  detector.collectorStopped("D", "maintenance", 2000);
  detector.collectorStarted("D", 3602000);

  ObservationGap gap = detector.lastGap("D");
  ASSERT_EQ(gap.duration_ms, 3600000);
  return true;
}

static bool test_gap_events_emitted() {
  ObservationGapDetector detector;
  std::size_t gap_events = 0;
  std::size_t health_events = 0;
  detector.setGapCallback([&](const GapEvent&) { gap_events++; });
  detector.setHealthCallback([&](const HealthEvent&) { health_events++; });

  detector.collectorStarted("E", 1000);
  detector.collectorStopped("E", "err", 2000);
  detector.collectorStarted("E", 5000);

  ASSERT_TRUE(gap_events >= static_cast<std::size_t>(1));
  ASSERT_TRUE(health_events >= static_cast<std::size_t>(2));
  return true;
}

static bool test_no_invented_events() {
  ObservationGapDetector detector;
  std::vector<GapEvent> gap_events;
  detector.setGapCallback([&](const GapEvent& e) { gap_events.push_back(e); });
  detector.setHealthCallback([](const HealthEvent&) {});

  detector.collectorStarted("F", 1000);
  detector.collectorStopped("F", "down", 2000);
  detector.collectorStarted("F", 5000);

  ASSERT_EQ(gap_events.size(), static_cast<std::size_t>(1));
  ASSERT_EQ(gap_events[0].started_at_ms, static_cast<std::int64_t>(2000));
  ASSERT_EQ(gap_events[0].ended_at_ms, static_cast<std::int64_t>(5000));
  return true;
}

static bool test_health_metrics_tracking() {
  CollectorHealthTracker tracker;
  tracker.recordEvent("Process", 100);
  tracker.recordEvent("Process", 200);
  tracker.recordDrop("Process");
  tracker.recordError("Process");
  tracker.recordSuccess("Process", 150);

  CollectorMetrics m = tracker.getMetrics("Process");
  ASSERT_EQ(m.events_generated, static_cast<std::size_t>(2));
  ASSERT_EQ(m.events_dropped, static_cast<std::size_t>(1));
  ASSERT_EQ(m.errors, static_cast<std::size_t>(1));
  ASSERT_EQ(m.latency_us, static_cast<std::int64_t>(150));
  ASSERT_TRUE(m.last_success_ms > 0);
  return true;
}

static bool test_health_latency() {
  CollectorHealthTracker tracker;
  tracker.recordEvent("Net", 500);
  CollectorMetrics m = tracker.getMetrics("Net");
  ASSERT_EQ(m.latency_us, static_cast<std::int64_t>(500));
  tracker.recordEvent("Net", 1000);
  m = tracker.getMetrics("Net");
  ASSERT_EQ(m.latency_us, static_cast<std::int64_t>(1000));
  return true;
}

static bool test_health_errors() {
  CollectorHealthTracker tracker;
  for (int i = 0; i < 10; i++) tracker.recordEvent("S");
  for (int i = 0; i < 3; i++) tracker.recordError("S");
  CollectorMetrics m = tracker.getMetrics("S");
  ASSERT_TRUE(m.errorRate() > 0.29 && m.errorRate() < 0.31);
  return true;
}

static bool test_health_drops() {
  CollectorHealthTracker tracker;
  for (int i = 0; i < 20; i++) tracker.recordEvent("D");
  for (int i = 0; i < 4; i++) tracker.recordDrop("D");
  CollectorMetrics m = tracker.getMetrics("D");
  ASSERT_TRUE(m.dropRate() > 0.19 && m.dropRate() < 0.21);
  return true;
}

static bool test_health_transitions() {
  CollectorHealthTracker tracker;
  std::size_t events = 0;
  tracker.setCallback([&](const HealthEvent&) { events++; });

  tracker.transition("Proc", CollectorHealthState::Started, "init");
  tracker.transition("Proc", CollectorHealthState::Running, "ready");
  tracker.transition("Proc", CollectorHealthState::Degraded, "high_latency");
  tracker.transition("Proc", CollectorHealthState::Running, "recovered");
  tracker.transition("Proc", CollectorHealthState::Stopped, "shutdown");

  ASSERT_TRUE(events >= static_cast<std::size_t>(5));
  ASSERT_EQ(tracker.getState("Proc"), CollectorHealthState::Stopped);
  return true;
}

static bool test_health_no_duplicate_transitions() {
  CollectorHealthTracker tracker;
  std::size_t events = 0;
  tracker.setCallback([&](const HealthEvent&) { events++; });

  tracker.transition("X", CollectorHealthState::Running, "start");
  tracker.transition("X", CollectorHealthState::Running, "same");
  ASSERT_EQ(events, static_cast<std::size_t>(1));
  return true;
}

static bool test_health_reset() {
  CollectorHealthTracker tracker;
  tracker.recordEvent("R", 100);
  tracker.recordError("R");
  tracker.transition("R", CollectorHealthState::Running, "init");
  tracker.reset("R");
  CollectorMetrics m = tracker.getMetrics("R");
  ASSERT_EQ(m.events_generated, static_cast<std::size_t>(0));
  ASSERT_EQ(tracker.getState("R"), CollectorHealthState::Unknown);
  return true;
}

static bool test_health_reset_all() {
  CollectorHealthTracker tracker;
  tracker.recordEvent("A");
  tracker.recordEvent("B");
  tracker.transition("A", CollectorHealthState::Running);
  tracker.transition("B", CollectorHealthState::Running);
  tracker.resetAll();
  ASSERT_EQ(tracker.getMetrics("A").events_generated, static_cast<std::size_t>(0));
  ASSERT_EQ(tracker.getMetrics("B").events_generated, static_cast<std::size_t>(0));
  return true;
}

static bool test_health_aggregated_metrics() {
  CollectorHealthTracker tracker;
  for (int i = 0; i < 100; i++) tracker.recordEvent("Agg", 10 + i);
  tracker.setQueueDepth("Agg", 50);
  tracker.setCpuUsage("Agg", 0.75);
  tracker.setMemoryUsage("Agg", 1024 * 1024);
  tracker.setObservationGapCount("Agg", 3);

  CollectorMetrics m = tracker.getMetrics("Agg");
  ASSERT_EQ(m.events_generated, static_cast<std::size_t>(100));
  ASSERT_EQ(m.queue_depth, static_cast<std::size_t>(50));
  ASSERT_TRUE(m.cpu_usage > 0.74 && m.cpu_usage < 0.76);
  ASSERT_EQ(m.memory_bytes, static_cast<std::size_t>(1024 * 1024));
  ASSERT_EQ(m.observation_gap_count, static_cast<std::size_t>(3));
  return true;
}

static bool test_restart_policy_config() {
  RestartPolicyConfig cfg;
  ASSERT_TRUE(cfg.isValid());
  cfg.auto_restart = true;
  cfg.max_restart_attempts = 0;
  ASSERT_FALSE(cfg.isValid());
  cfg.max_restart_attempts = 3;
  cfg.initial_backoff_ms = 0;
  ASSERT_FALSE(cfg.isValid());
  return true;
}

static bool test_restart_enabled() {
  RestartPolicyConfig cfg;
  cfg.auto_restart = true;
  cfg.max_restart_attempts = 3;
  cfg.initial_backoff_ms = 1000;
  cfg.backoff_multiplier = 2.0;
  cfg.max_backoff_ms = 30000;

  RestartPolicy policy(cfg);
  RestartDecision d1 = policy.recordFailure("P");
  ASSERT_TRUE(d1.shouldRestart());
  ASSERT_EQ(d1.attempt, static_cast<std::size_t>(1));
  ASSERT_EQ(d1.backoff_ms, static_cast<std::int64_t>(1000));
  return true;
}

static bool test_restart_backoff() {
  RestartPolicyConfig cfg;
  cfg.auto_restart = true;
  cfg.max_restart_attempts = 5;
  cfg.initial_backoff_ms = 1000;
  cfg.backoff_multiplier = 2.0;
  cfg.max_backoff_ms = 30000;

  RestartPolicy policy(cfg);
  RestartDecision d1 = policy.recordFailure("B");
  ASSERT_EQ(d1.backoff_ms, static_cast<std::int64_t>(1000));

  RestartDecision d2 = policy.recordFailure("B");
  ASSERT_EQ(d2.backoff_ms, static_cast<std::int64_t>(2000));

  RestartDecision d3 = policy.recordFailure("B");
  ASSERT_EQ(d3.backoff_ms, static_cast<std::int64_t>(4000));

  RestartDecision d4 = policy.recordFailure("B");
  ASSERT_EQ(d4.backoff_ms, static_cast<std::int64_t>(8000));
  return true;
}

static bool test_restart_max_attempts() {
  RestartPolicyConfig cfg;
  cfg.auto_restart = true;
  cfg.max_restart_attempts = 2;
  cfg.initial_backoff_ms = 100;

  RestartPolicy policy(cfg);
  RestartDecision d1 = policy.recordFailure("M");
  ASSERT_TRUE(d1.shouldRestart());
  RestartDecision d2 = policy.recordFailure("M");
  ASSERT_TRUE(d2.shouldRestart());
  RestartDecision d3 = policy.recordFailure("M");
  ASSERT_FALSE(d3.shouldRestart());
  ASSERT_TRUE(policy.isExhausted("M"));
  return true;
}

static bool test_restart_disabled() {
  RestartPolicyConfig cfg;
  cfg.auto_restart = false;
  cfg.initial_backoff_ms = 1000;

  RestartPolicy policy(cfg);
  RestartDecision d = policy.recordFailure("D");
  ASSERT_FALSE(d.shouldRestart());
  ASSERT_EQ(d.action, RestartAction::None);
  return true;
}

static bool test_restart_successful_recovery() {
  RestartPolicyConfig cfg;
  cfg.auto_restart = true;
  cfg.max_restart_attempts = 3;
  cfg.initial_backoff_ms = 1000;

  RestartPolicy policy(cfg);
  policy.recordFailure("R");
  policy.recordFailure("R");
  policy.recordSuccess("R");
  ASSERT_FALSE(policy.isExhausted("R"));
  ASSERT_EQ(policy.restartCount("R"), static_cast<std::size_t>(2));

  RestartDecision d = policy.recordFailure("R");
  ASSERT_TRUE(d.shouldRestart());
  ASSERT_EQ(d.attempt, static_cast<std::size_t>(1));
  return true;
}

static bool test_restart_backoff_capped() {
  RestartPolicyConfig cfg;
  cfg.auto_restart = true;
  cfg.max_restart_attempts = 20;
  cfg.initial_backoff_ms = 1000;
  cfg.backoff_multiplier = 10.0;
  cfg.max_backoff_ms = 5000;

  RestartPolicy policy(cfg);
  for (int i = 0; i < 10; i++) policy.recordFailure("C");
  ASSERT_TRUE(policy.lastBackoffMs("C") <= 5000);
  return true;
}

static bool test_restart_reset() {
  RestartPolicyConfig cfg;
  cfg.auto_restart = true;
  cfg.max_restart_attempts = 2;
  cfg.initial_backoff_ms = 100;

  RestartPolicy policy(cfg);
  policy.recordFailure("Z");
  policy.recordFailure("Z");
  policy.recordFailure("Z");
  ASSERT_TRUE(policy.isExhausted("Z"));

  policy.reset("Z");
  ASSERT_FALSE(policy.isExhausted("Z"));
  ASSERT_EQ(policy.restartCount("Z"), static_cast<std::size_t>(0));
  return true;
}

static bool test_restart_disabled_never_restarts() {
  RestartPolicyConfig cfg;
  cfg.auto_restart = false;

  RestartPolicy policy(cfg);
  for (int i = 0; i < 100; i++) {
    RestartDecision d = policy.recordFailure("N");
    ASSERT_FALSE(d.shouldRestart());
  }
  return true;
}

static bool test_health_event_action_mapping() {
  HealthEvent e;
  e.collector = "Test";
  e.state = CollectorHealthState::Started;
  ASSERT_EQ(e.action(), "collector.started");
  e.state = CollectorHealthState::Degraded;
  ASSERT_EQ(e.action(), "collector.degraded");
  return true;
}

#ifndef MONIX_KERNEL_BUILD
int main() {
  std::cerr << "=== MONIX Health & Restart Tests ===\n\n";

  RUN_TEST(gap_event_names);
  RUN_TEST(health_state_names);
  RUN_TEST(gap_validity);
  RUN_TEST(health_event_action);
  RUN_TEST(metrics_validity);
  RUN_TEST(metrics_no_division_by_zero);
  RUN_TEST(gap_detector_lifecycle);
  RUN_TEST(gap_duration);
  RUN_TEST(multiple_gaps);
  RUN_TEST(short_gap);
  RUN_TEST(long_gap);
  RUN_TEST(gap_events_emitted);
  RUN_TEST(no_invented_events);
  RUN_TEST(health_metrics_tracking);
  RUN_TEST(health_latency);
  RUN_TEST(health_errors);
  RUN_TEST(health_drops);
  RUN_TEST(health_transitions);
  RUN_TEST(health_no_duplicate_transitions);
  RUN_TEST(health_reset);
  RUN_TEST(health_reset_all);
  RUN_TEST(health_aggregated_metrics);
  RUN_TEST(restart_policy_config);
  RUN_TEST(restart_enabled);
  RUN_TEST(restart_backoff);
  RUN_TEST(restart_max_attempts);
  RUN_TEST(restart_disabled);
  RUN_TEST(restart_successful_recovery);
  RUN_TEST(restart_backoff_capped);
  RUN_TEST(restart_reset);
  RUN_TEST(restart_disabled_never_restarts);
  RUN_TEST(health_event_action_mapping);

  int passed = 0, failed = 0;
  for (const auto& r : results) {
    if (r.passed) passed++; else failed++;
  }

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
  {"Gap event names", test_gap_event_names},
  {"Health state names", test_health_state_names},
  {"Gap validity", test_gap_validity},
  {"Health event action", test_health_event_action},
  {"Metrics validity", test_metrics_validity},
  {"Metrics no division by zero", test_metrics_no_division_by_zero},
  {"Gap detector lifecycle", test_gap_detector_lifecycle},
  {"Gap duration", test_gap_duration},
  {"Multiple gaps", test_multiple_gaps},
  {"Short gap", test_short_gap},
  {"Long gap", test_long_gap},
  {"Gap events emitted", test_gap_events_emitted},
  {"No invented events", test_no_invented_events},
  {"Health metrics tracking", test_health_metrics_tracking},
  {"Health latency", test_health_latency},
  {"Health errors", test_health_errors},
  {"Health drops", test_health_drops},
  {"Health transitions", test_health_transitions},
  {"Health no duplicate transitions", test_health_no_duplicate_transitions},
  {"Health reset", test_health_reset},
  {"Health reset all", test_health_reset_all},
  {"Health aggregated metrics", test_health_aggregated_metrics},
  {"Restart policy config", test_restart_policy_config},
  {"Restart enabled", test_restart_enabled},
  {"Restart backoff", test_restart_backoff},
  {"Restart max attempts", test_restart_max_attempts},
  {"Restart disabled", test_restart_disabled},
  {"Restart successful recovery", test_restart_successful_recovery},
  {"Restart backoff capped", test_restart_backoff_capped},
  {"Restart reset", test_restart_reset},
  {"Restart disabled never restarts", test_restart_disabled_never_restarts},
  {"Health event action mapping", test_health_event_action_mapping},
};

const KBoolTestEntry* GetKBoolTests_Health() { return s_kbooltests; }
std::size_t GetKBoolTestCount_Health() { return sizeof(s_kbooltests) / sizeof(s_kbooltests[0]); }
#endif
