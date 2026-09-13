#include <cassert>
#include <chrono>
#include <cmath>
#include <functional>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <sstream>
#include <iomanip>
#include <unordered_map>
#include <numeric>
#include <algorithm>
#include <random>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "RuleEngine.hpp"
#include "DetectionContext.hpp"
#include "SelfMonitor.hpp"

using namespace monix::collectors::ruleengine;
using namespace monix::collectors::context;
using namespace monix::collectors::selfmonitor;

struct TestResult { std::string name; bool passed; double elapsed_ms = 0; };
static std::vector<TestResult> results;

#define RUN_TEST(name) do { \
  std::cerr << "  " << #name << "... "; \
  auto _start = std::chrono::high_resolution_clock::now(); \
  bool ok = false; \
  try { ok = test_##name(); } catch (...) { ok = false; } \
  auto _end = std::chrono::high_resolution_clock::now(); \
  double _ms = std::chrono::duration<double, std::milli>(_end - _start).count(); \
  results.push_back({#name, ok, _ms}); \
  if (ok) std::cerr << "[PASS] (" << std::fixed << std::setprecision(1) << _ms << "ms)\n"; \
  else std::cerr << "[FAIL]\n"; \
} while(0)

#define ASSERT_TRUE(expr) do { if (!(expr)) { std::cerr << "\n    FAIL: " << #expr << " (line " << __LINE__ << ")"; return false; } } while(0)
#define ASSERT_FALSE(expr) ASSERT_TRUE(!(expr))
#define ASSERT_EQ(a, b) do { if ((a) != (b)) { std::cerr << "\n    FAIL: " << #a << " != " << #b << " (line " << __LINE__ << ")"; return false; } } while(0)

// ===== Phase 40: Rule Engine Tests =====

static bool test_re_add_rule() {
  RuleEngine engine;
  Rule rule;
  rule.rule_id = "R1";
  rule.name = "Unknown Device Rule";
  rule.conditions.push_back({"device_trust", RuleOperator::Equals, "unknown", {}});
  RuleEffect effect;
  effect.action = RuleAction::GenerateAnalysis;
  effect.analysis_type = "unknown_device";
  effect.priority_override = "HIGH";
  rule.effects.push_back(effect);
  engine.addRule(rule);
  ASSERT_EQ(engine.ruleCount(), static_cast<std::size_t>(1));
  return true;
}

static bool test_re_evaluate_match() {
  RuleEngine engine;
  Rule rule;
  rule.rule_id = "R1";
  rule.name = "Unknown Device";
  rule.conditions.push_back({"device_trust", RuleOperator::Equals, "unknown", {}});
  RuleEffect effect;
  effect.action = RuleAction::GenerateAnalysis;
  effect.analysis_type = "unknown_device";
  effect.priority_override = "HIGH";
  rule.effects.push_back(effect);
  engine.addRule(rule);

  ObservedEvent event;
  event.event_id = "E1";
  event.event_type = "device.connected";
  event.device_trust = "unknown";
  event.timestamp_ms = 1000;

  auto result = engine.evaluate(event, 1000);
  ASSERT_TRUE(result.matched || result.rules_matched > 0);
  ASSERT_TRUE(result.generated.size() >= static_cast<std::size_t>(1));
  ASSERT_EQ(result.generated[0].analysis_type, "unknown_device");
  ASSERT_EQ(result.generated[0].priority, "HIGH");
  ASSERT_EQ(result.generated[0].source_event_id, "E1");
  return true;
}

static bool test_re_evaluate_no_match() {
  RuleEngine engine;
  Rule rule;
  rule.rule_id = "R1";
  rule.name = "Unknown Device";
  rule.conditions.push_back({"device_trust", RuleOperator::Equals, "unknown", {}});
  RuleEffect effect;
  effect.action = RuleAction::GenerateAnalysis;
  effect.analysis_type = "unknown_device";
  rule.effects.push_back(effect);
  engine.addRule(rule);

  ObservedEvent event;
  event.event_id = "E1";
  event.event_type = "device.connected";
  event.device_trust = "trusted";
  event.timestamp_ms = 1000;

  auto result = engine.evaluate(event, 1000);
  ASSERT_TRUE(result.generated.empty());
  return true;
}

static bool test_re_multiple_conditions() {
  RuleEngine engine;
  Rule rule;
  rule.rule_id = "R1";
  rule.name = "External Unknown Device";
  rule.conditions.push_back({"device_trust", RuleOperator::Equals, "unknown", {}});
  rule.conditions.push_back({"device_type", RuleOperator::Equals, "usb", {}});
  RuleEffect effect;
  effect.action = RuleAction::GenerateAnalysis;
  effect.analysis_type = "external_unknown";
  effect.priority_override = "HIGH";
  rule.effects.push_back(effect);
  engine.addRule(rule);

  ObservedEvent event;
  event.event_id = "E1";
  event.event_type = "device.connected";
  event.device_trust = "unknown";
  event.device_type = "usb";
  event.timestamp_ms = 1000;

  auto result = engine.evaluate(event, 1000);
  ASSERT_TRUE(result.generated.size() >= static_cast<std::size_t>(1));
  return true;
}

static bool test_re_no_original_modified() {
  RuleEngine engine;
  Rule rule;
  rule.rule_id = "R1";
  rule.name = "Tag Rule";
  rule.conditions.push_back({"severity", RuleOperator::Equals, "Critical", {}});
  RuleEffect effect;
  effect.action = RuleAction::TagEvent;
  effect.tag = "needs_review";
  rule.effects.push_back(effect);
  engine.addRule(rule);

  ObservedEvent event;
  event.event_id = "E1";
  event.event_type = "process.start";
  event.severity = "Critical";
  event.timestamp_ms = 1000;

  std::string original_type = event.event_type;
  auto result = engine.evaluate(event, 1000);
  ASSERT_EQ(event.event_type, original_type);
  return true;
}

static bool test_re_cooldown() {
  RuleEngine engine;
  Rule rule;
  rule.rule_id = "R1";
  rule.name = "Cooldown Rule";
  rule.cooldown_ms = 5000;
  rule.conditions.push_back({"event_type", RuleOperator::Equals, "alert", {}});
  RuleEffect effect;
  effect.action = RuleAction::GenerateAnalysis;
  effect.analysis_type = "alert_analysis";
  rule.effects.push_back(effect);
  engine.addRule(rule);

  ObservedEvent event;
  event.event_id = "E1";
  event.event_type = "alert";
  event.timestamp_ms = 1000;

  auto r1 = engine.evaluate(event, 1000);
  ASSERT_TRUE(r1.generated.size() >= static_cast<std::size_t>(1));

  auto r2 = engine.evaluate(event, 2000);
  ASSERT_TRUE(r2.generated.empty());

  auto r3 = engine.evaluate(event, 6000);
  ASSERT_TRUE(r3.generated.size() >= static_cast<std::size_t>(1));
  return true;
}

static bool test_re_contains_operator() {
  RuleEngine engine;
  Rule rule;
  rule.rule_id = "R1";
  rule.name = "Path Contains";
  rule.conditions.push_back({"payload", RuleOperator::Contains, "temp", {}});
  RuleEffect effect;
  effect.action = RuleAction::GenerateAnalysis;
  effect.analysis_type = "temp_file";
  rule.effects.push_back(effect);
  engine.addRule(rule);

  ObservedEvent event;
  event.event_id = "E1";
  event.event_type = "file.create";
  event.payload = "C:\\Users\\temp\\file.txt";
  event.timestamp_ms = 1000;

  auto result = engine.evaluate(event, 1000);
  ASSERT_TRUE(result.generated.size() >= static_cast<std::size_t>(1));
  return true;
}

static bool test_re_not_equals_operator() {
  RuleEngine engine;
  Rule rule;
  rule.rule_id = "R1";
  rule.name = "Non-system process";
  rule.conditions.push_back({"actor", RuleOperator::NotEquals, "SYSTEM", {}});
  RuleEffect effect;
  effect.action = RuleAction::GenerateAnalysis;
  effect.analysis_type = "user_process";
  rule.effects.push_back(effect);
  engine.addRule(rule);

  ObservedEvent event;
  event.event_id = "E1";
  event.event_type = "process.start";
  event.actor = "user123";
  event.timestamp_ms = 1000;

  auto result = engine.evaluate(event, 1000);
  ASSERT_TRUE(result.generated.size() >= static_cast<std::size_t>(1));
  return true;
}

static bool test_re_derived_operator() {
  RuleEngine engine;
  Rule rule;
  rule.rule_id = "R1";
  rule.name = "Has reputation";
  rule.conditions.push_back({"reputation", RuleOperator::Derived, "", {}});
  RuleEffect effect;
  effect.action = RuleAction::GenerateAnalysis;
  effect.analysis_type = "reputation_check";
  rule.effects.push_back(effect);
  engine.addRule(rule);

  ObservedEvent event;
  event.event_id = "E1";
  event.event_type = "process.start";
  event.fields["reputation"] = "known_good";
  event.timestamp_ms = 1000;

  auto result = engine.evaluate(event, 1000);
  ASSERT_TRUE(result.generated.size() >= static_cast<std::size_t>(1));
  return true;
}

// ===== Phase 41: Detection Context Tests =====

static bool test_ctx_process() {
  DetectionContextBuilder builder;
  ProcessContext proc;
  proc.process_name = "cmd.exe";
  proc.pid = 1234;
  proc.is_signed = true;
  builder.setProcess(proc);
  auto ctx = builder.build();
  ASSERT_TRUE(ctx.process.isValid());
  ASSERT_EQ(ctx.process.process_name, "cmd.exe");
  ASSERT_EQ(ctx.process.pid, static_cast<std::uint64_t>(1234));
  return true;
}

static bool test_ctx_file() {
  DetectionContextBuilder builder;
  FileContext file;
  file.file_path = "C:\\Windows\\System32\\cmd.exe";
  file.extension = ".exe";
  file.is_executable = true;
  builder.setFile(file);
  auto ctx = builder.build();
  ASSERT_TRUE(ctx.file.isValid());
  ASSERT_TRUE(ctx.file.is_executable);
  return true;
}

static bool test_ctx_device() {
  DetectionContextBuilder builder;
  DeviceContext device;
  device.device_id = "USB001";
  device.device_type = "usb";
  device.is_external = true;
  device.trust_level = "unknown";
  builder.setDevice(device);
  auto ctx = builder.build();
  ASSERT_TRUE(ctx.device.isValid());
  ASSERT_TRUE(ctx.device.is_external);
  return true;
}

static bool test_ctx_network() {
  DetectionContextBuilder builder;
  NetworkContext net;
  net.source_ip = "192.168.1.100";
  net.dest_ip = "10.0.0.1";
  net.dest_port = 443;
  net.protocol = "tcp";
  net.is_encrypted = true;
  builder.setNetwork(net);
  auto ctx = builder.build();
  ASSERT_TRUE(ctx.network.isValid());
  ASSERT_TRUE(ctx.network.is_encrypted);
  return true;
}

static bool test_ctx_user() {
  DetectionContextBuilder builder;
  UserContext user;
  user.user_id = "U001";
  user.user_name = "admin";
  user.is_admin = true;
  builder.setUser(user);
  auto ctx = builder.build();
  ASSERT_TRUE(ctx.user.isValid());
  ASSERT_TRUE(ctx.user.is_admin);
  return true;
}

static bool test_ctx_extra_derived() {
  DetectionContextBuilder builder;
  builder.addExtra("process_reputation", "known_good", ContextSource::Derived, ContextConfidence::High);
  builder.addExtra("file_risk_score", "0.2", ContextSource::Inferred, ContextConfidence::Medium);
  auto ctx = builder.build();
  ASSERT_TRUE(ctx.hasExtra("process_reputation"));
  ASSERT_EQ(ctx.getExtra("process_reputation"), "known_good");
  ASSERT_EQ(ctx.entryCount(), static_cast<std::size_t>(2));
  return true;
}

static bool test_ctx_not_contaminate_original() {
  DetectionContextBuilder builder;
  ProcessContext proc;
  proc.process_name = "original.exe";
  builder.setProcess(proc);
  auto ctx = builder.build();
  ASSERT_EQ(ctx.process.process_name, "original.exe");
  builder.clear();
  auto ctx2 = builder.build();
  ASSERT_FALSE(ctx2.process.isValid());
  return true;
}

static bool test_ctx_confidence_levels() {
  DetectionContextBuilder builder;
  builder.addExtra("certain_fact", "true", ContextSource::Direct, ContextConfidence::Certain);
  builder.addExtra("speculative", "maybe", ContextSource::Inferred, ContextConfidence::Speculative);
  auto ctx = builder.build();
  ASSERT_TRUE(ctx.extra.at("certain_fact").confidence == ContextConfidence::Certain);
  ASSERT_TRUE(ctx.extra.at("speculative").confidence == ContextConfidence::Speculative);
  return true;
}

// ===== Phase 42: Self-Monitoring Tests =====

static bool test_sm_record_metric() {
  SelfMonitor monitor;
  monitor.recordMetric("test_counter", 1.0, MetricType::Counter);
  monitor.recordMetric("test_counter", 2.0, MetricType::Counter);
  auto m = monitor.getMetric("test_counter");
  ASSERT_EQ(m.count, static_cast<std::size_t>(2));
  ASSERT_TRUE(std::abs(m.sum - 3.0) < 0.001);
  return true;
}

static bool test_sm_snapshot() {
  SelfMonitor monitor;
  monitor.setEventBusLatency(1.5);
  monitor.setQueueDepth(100);
  monitor.setEventsPerSec(5000.0);
  monitor.addDroppedEvents(10);

  auto snap = monitor.snapshot();
  ASSERT_TRUE(std::abs(snap.eventbus_latency_ms - 1.5) < 0.001);
  ASSERT_EQ(snap.queue_depth, static_cast<std::size_t>(100));
  ASSERT_TRUE(std::abs(snap.events_per_sec - 5000.0) < 0.001);
  ASSERT_EQ(snap.dropped_events, static_cast<std::size_t>(10));
  return true;
}

static bool test_sm_health_healthy() {
  SelfMonitor monitor;
  monitor.setCollectorStatus("fs", HealthStatus::Healthy);
  monitor.setCollectorStatus("proc", HealthStatus::Healthy);
  auto h = monitor.health();
  ASSERT_EQ(h.overall, HealthStatus::Healthy);
  ASSERT_EQ(h.unhealthy_components, static_cast<std::size_t>(0));
  return true;
}

static bool test_sm_health_degraded() {
  SelfMonitor monitor;
  monitor.setCollectorStatus("fs", HealthStatus::Healthy);
  monitor.setCollectorStatus("proc", HealthStatus::Degraded);
  auto h = monitor.health();
  ASSERT_TRUE(h.overall >= HealthStatus::Degraded);
  return true;
}

static bool test_sm_health_unhealthy_storage() {
  SelfMonitor monitor;
  monitor.addStorageFailure(200);
  auto h = monitor.health();
  ASSERT_EQ(h.storage, HealthStatus::Unhealthy);
  return true;
}

static bool test_sm_health_unhealthy_drops() {
  SelfMonitor monitor;
  monitor.addDroppedEvents(20000);
  auto h = monitor.health();
  ASSERT_EQ(h.eventbus, HealthStatus::Unhealthy);
  return true;
}

static bool test_sm_quarantine_tracking() {
  SelfMonitor monitor;
  monitor.addQuarantineEntry(5);
  auto snap = monitor.snapshot();
  ASSERT_EQ(snap.quarantine_size, static_cast<std::size_t>(5));
  monitor.removeQuarantineEntry(3);
  snap = monitor.snapshot();
  ASSERT_EQ(snap.quarantine_size, static_cast<std::size_t>(2));
  return true;
}

static bool test_sm_aggregated_metrics() {
  SelfMonitor monitor;
  for (int i = 1; i <= 10; i++) {
    monitor.recordMetric("latency", static_cast<double>(i), MetricType::Histogram);
  }
  auto m = monitor.getMetric("latency");
  ASSERT_EQ(m.count, static_cast<std::size_t>(10));
  ASSERT_TRUE(std::abs(m.avg - 5.5) < 0.001);
  ASSERT_TRUE(std::abs(m.min_val - 1.0) < 0.001);
  ASSERT_TRUE(std::abs(m.max_val - 10.0) < 0.001);
  return true;
}

static bool test_sm_all_metrics() {
  SelfMonitor monitor;
  monitor.recordMetric("m1", 1.0);
  monitor.recordMetric("m2", 2.0);
  monitor.recordMetric("m3", 3.0);
  auto all = monitor.allMetrics();
  ASSERT_EQ(all.size(), static_cast<std::size_t>(3));
  return true;
}

// ===== Phase 43: Performance Benchmark Tests =====

static double measureOpsPerSec(std::function<void()> op, int iterations) {
  auto start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < iterations; i++) op();
  auto end = std::chrono::high_resolution_clock::now();
  double elapsed_sec = std::chrono::duration<double>(end - start).count();
  return static_cast<double>(iterations) / elapsed_sec;
}

static bool test_perf_event_creation() {
  double ops = measureOpsPerSec([]() {
    ObservedEvent e;
    e.event_id = "E1";
    e.event_type = "test.event";
    e.source = "benchmark";
    e.timestamp_ms = 1000;
    volatile bool valid = e.isValid();
    (void)valid;
  }, 100000);
  std::cerr << "\n    [event_creation: " << std::fixed << std::setprecision(0) << ops << " ops/s] ";
  ASSERT_TRUE(ops > 100000.0);
  return true;
}

static bool test_perf_rule_evaluation() {
  RuleEngine engine;
  Rule rule;
  rule.rule_id = "R1";
  rule.name = "Benchmark Rule";
  rule.conditions.push_back({"device_trust", RuleOperator::Equals, "unknown", {}});
  RuleEffect effect;
  effect.action = RuleAction::GenerateAnalysis;
  effect.analysis_type = "bench";
  rule.effects.push_back(effect);
  engine.addRule(rule);

  ObservedEvent event;
  event.event_id = "E1";
  event.event_type = "device.connected";
  event.device_trust = "unknown";
  event.timestamp_ms = 1000;

  double ops = measureOpsPerSec([&]() {
    engine.evaluate(event, 1000);
  }, 100000);
  std::cerr << "\n    [rule_eval: " << std::fixed << std::setprecision(0) << ops << " ops/s] ";
  ASSERT_TRUE(ops > 10000.0);
  return true;
}

static bool test_perf_context_build() {
  DetectionContextBuilder builder;
  ProcessContext proc;
  proc.process_name = "bench.exe";
  proc.pid = 9999;
  builder.setProcess(proc);

  double ops = measureOpsPerSec([&]() {
    builder.clear();
    builder.setProcess(proc);
    auto ctx = builder.build();
    volatile bool valid = ctx.isValid();
    (void)valid;
  }, 100000);
  std::cerr << "\n    [context_build: " << std::fixed << std::setprecision(0) << ops << " ops/s] ";
  ASSERT_TRUE(ops > 50000.0);
  return true;
}

static bool test_perf_self_monitor_record() {
  SelfMonitor monitor;
  double ops = measureOpsPerSec([&]() {
    monitor.recordMetric("bench", 1.0, MetricType::Gauge);
  }, 100000);
  std::cerr << "\n    [monitor_record: " << std::fixed << std::setprecision(0) << ops << " ops/s] ";
  ASSERT_TRUE(ops > 50000.0);
  return true;
}

static bool test_perf_condition_matching() {
  RuleCondition cond;
  cond.field = "severity";
  cond.op = RuleOperator::Equals;
  cond.value = "High";

  double ops = measureOpsPerSec([&]() {
    volatile bool r = cond.matches("High");
    (void)r;
  }, 1000000);
  std::cerr << "\n    [condition_match: " << std::fixed << std::setprecision(0) << ops << " ops/s] ";
  ASSERT_TRUE(ops > 500000.0);
  return true;
}

// ===== Phase 44: Stress Tests =====

static bool test_stress_many_rules() {
  RuleEngine engine;
  for (int i = 0; i < 1000; i++) {
    Rule rule;
    rule.rule_id = "R" + std::to_string(i);
    rule.name = "Rule " + std::to_string(i);
    rule.conditions.push_back({"event_type", RuleOperator::Equals, "event." + std::to_string(i % 100), {}});
    RuleEffect effect;
    effect.action = RuleAction::GenerateAnalysis;
    effect.analysis_type = "analysis_" + std::to_string(i);
    rule.effects.push_back(effect);
    engine.addRule(rule);
  }
  ASSERT_EQ(engine.ruleCount(), static_cast<std::size_t>(1000));

  ObservedEvent event;
  event.event_id = "E1";
  event.event_type = "event.50";
  event.timestamp_ms = 1000;

  auto start = std::chrono::high_resolution_clock::now();
  auto result = engine.evaluate(event, 1000);
  auto end = std::chrono::high_resolution_clock::now();
  double ms = std::chrono::duration<double, std::milli>(end - start).count();

  ASSERT_TRUE(result.rules_matched >= static_cast<std::size_t>(1));
  ASSERT_TRUE(ms < 100.0);
  return true;
}

static bool test_stress_many_events() {
  RuleEngine engine;
  Rule rule;
  rule.rule_id = "R1";
  rule.name = "High Volume";
  rule.conditions.push_back({"severity", RuleOperator::Equals, "Critical", {}});
  RuleEffect effect;
  effect.action = RuleAction::GenerateAnalysis;
  effect.analysis_type = "critical_analysis";
  rule.effects.push_back(effect);
  engine.addRule(rule);

  auto start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < 100000; i++) {
    ObservedEvent event;
    event.event_id = "E" + std::to_string(i);
    event.event_type = "test.event";
    event.severity = (i % 10 == 0) ? "Critical" : "Info";
    event.timestamp_ms = i;
    engine.evaluate(event, i);
  }
  auto end = std::chrono::high_resolution_clock::now();
  double ms = std::chrono::duration<double, std::milli>(end - start).count();

  ASSERT_TRUE(engine.totalMatches() >= static_cast<std::size_t>(10000));
  ASSERT_TRUE(ms < 5000.0);
  return true;
}

static bool test_stress_context_creation() {
  DetectionContextBuilder builder;
  auto start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < 100000; i++) {
    ProcessContext proc;
    proc.process_name = "stress_" + std::to_string(i) + ".exe";
    proc.pid = i;
    builder.setProcess(proc);
    auto ctx = builder.build();
    volatile bool v = ctx.isValid();
    (void)v;
  }
  auto end = std::chrono::high_resolution_clock::now();
  double ms = std::chrono::duration<double, std::milli>(end - start).count();
  ASSERT_TRUE(ms < 3000.0);
  return true;
}

static bool test_stress_monitor_aggregation() {
  SelfMonitor monitor;
  auto start = std::chrono::high_resolution_clock::now();
  for (int i = 0; i < 100000; i++) {
    monitor.recordMetric("stress_metric", static_cast<double>(i % 1000), MetricType::Histogram);
    if (i % 100 == 0) {
      monitor.setQueueDepth(i);
      monitor.addDroppedEvents(1);
    }
  }
  auto end = std::chrono::high_resolution_clock::now();
  double ms = std::chrono::duration<double, std::milli>(end - start).count();
  auto m = monitor.getMetric("stress_metric");
  ASSERT_TRUE(m.count == static_cast<std::size_t>(100000));
  ASSERT_TRUE(ms < 3000.0);
  return true;
}

static bool test_stress_memory_pressure() {
  RuleEngine engine;
  for (int i = 0; i < 5000; i++) {
    Rule rule;
    rule.rule_id = "R" + std::to_string(i);
    rule.name = "Memory Rule " + std::to_string(i);
    rule.conditions.push_back({"event_type", RuleOperator::Equals, "type_" + std::to_string(i % 500), {}});
    RuleEffect effect;
    effect.action = RuleAction::GenerateAnalysis;
    effect.analysis_type = "analysis_" + std::to_string(i);
    rule.effects.push_back(effect);
    engine.addRule(rule);
  }
  ASSERT_EQ(engine.ruleCount(), static_cast<std::size_t>(5000));

  for (int i = 0; i < 50000; i++) {
    ObservedEvent event;
    event.event_id = "MP" + std::to_string(i);
    event.event_type = "type_" + std::to_string(i % 500);
    event.severity = "Info";
    event.timestamp_ms = i;
    engine.evaluate(event, i);
  }
  ASSERT_TRUE(engine.totalEvaluations() >= static_cast<std::size_t>(50000));
  return true;
}

#ifndef MONIX_KERNEL_BUILD
int main() {
  std::cerr << "=== MONIX Phases 40-44 Tests ===\n\n";

  std::cerr << "--- Phase 40: Rule Engine ---\n";
  RUN_TEST(re_add_rule);
  RUN_TEST(re_evaluate_match);
  RUN_TEST(re_evaluate_no_match);
  RUN_TEST(re_multiple_conditions);
  RUN_TEST(re_no_original_modified);
  RUN_TEST(re_cooldown);
  RUN_TEST(re_contains_operator);
  RUN_TEST(re_not_equals_operator);
  RUN_TEST(re_derived_operator);

  std::cerr << "\n--- Phase 41: Detection Context ---\n";
  RUN_TEST(ctx_process);
  RUN_TEST(ctx_file);
  RUN_TEST(ctx_device);
  RUN_TEST(ctx_network);
  RUN_TEST(ctx_user);
  RUN_TEST(ctx_extra_derived);
  RUN_TEST(ctx_not_contaminate_original);
  RUN_TEST(ctx_confidence_levels);

  std::cerr << "\n--- Phase 42: Self-Monitoring ---\n";
  RUN_TEST(sm_record_metric);
  RUN_TEST(sm_snapshot);
  RUN_TEST(sm_health_healthy);
  RUN_TEST(sm_health_degraded);
  RUN_TEST(sm_health_unhealthy_storage);
  RUN_TEST(sm_health_unhealthy_drops);
  RUN_TEST(sm_quarantine_tracking);
  RUN_TEST(sm_aggregated_metrics);
  RUN_TEST(sm_all_metrics);

  std::cerr << "\n--- Phase 43: Performance Benchmarks ---\n";
  RUN_TEST(perf_event_creation);
  RUN_TEST(perf_rule_evaluation);
  RUN_TEST(perf_context_build);
  RUN_TEST(perf_self_monitor_record);
  RUN_TEST(perf_condition_matching);

  std::cerr << "\n--- Phase 44: Stress Tests ---\n";
  RUN_TEST(stress_many_rules);
  RUN_TEST(stress_many_events);
  RUN_TEST(stress_context_creation);
  RUN_TEST(stress_monitor_aggregation);
  RUN_TEST(stress_memory_pressure);

  int passed = 0, failed = 0;
  double total_ms = 0;
  for (const auto& r : results) {
    if (r.passed) passed++; else failed++;
    total_ms += r.elapsed_ms;
  }
  std::cerr << "\n=== Results: " << passed << " passed, " << failed << " failed (total " << std::fixed << std::setprecision(1) << total_ms << "ms) ===\n";
  return failed > 0 ? 1 : 0;
}
#endif

#ifdef MONIX_KERNEL_BUILD

struct KBoolTestEntry {
  const char* display_name;
  bool (*func)();
};

static const KBoolTestEntry s_kbooltests[] = {
  {"Rule engine add rule", test_re_add_rule},
  {"Rule engine evaluate match", test_re_evaluate_match},
  {"Rule engine evaluate no match", test_re_evaluate_no_match},
  {"Rule engine multiple conditions", test_re_multiple_conditions},
  {"Rule engine no original modified", test_re_no_original_modified},
  {"Rule engine cooldown", test_re_cooldown},
  {"Rule engine contains operator", test_re_contains_operator},
  {"Rule engine not equals operator", test_re_not_equals_operator},
  {"Rule engine derived operator", test_re_derived_operator},
  {"Context process", test_ctx_process},
  {"Context file", test_ctx_file},
  {"Context device", test_ctx_device},
  {"Context network", test_ctx_network},
  {"Context user", test_ctx_user},
  {"Context extra derived", test_ctx_extra_derived},
  {"Context not contaminate original", test_ctx_not_contaminate_original},
  {"Context confidence levels", test_ctx_confidence_levels},
  {"Self monitor record metric", test_sm_record_metric},
  {"Self monitor snapshot", test_sm_snapshot},
  {"Self monitor health healthy", test_sm_health_healthy},
  {"Self monitor health degraded", test_sm_health_degraded},
  {"Self monitor unhealthy storage", test_sm_health_unhealthy_storage},
  {"Self monitor unhealthy drops", test_sm_health_unhealthy_drops},
  {"Self monitor quarantine tracking", test_sm_quarantine_tracking},
  {"Self monitor aggregated metrics", test_sm_aggregated_metrics},
  {"Self monitor all metrics", test_sm_all_metrics},
  {"Perf event creation", test_perf_event_creation},
  {"Perf rule evaluation", test_perf_rule_evaluation},
  {"Perf context build", test_perf_context_build},
  {"Perf self monitor record", test_perf_self_monitor_record},
  {"Perf condition matching", test_perf_condition_matching},
  {"Stress many rules", test_stress_many_rules},
  {"Stress many events", test_stress_many_events},
  {"Stress context creation", test_stress_context_creation},
  {"Stress monitor aggregation", test_stress_monitor_aggregation},
  {"Stress memory pressure", test_stress_memory_pressure},
};

const KBoolTestEntry* GetKBoolTests_Phases40_44() { return s_kbooltests; }
std::size_t GetKBoolTestCount_Phases40_44() { return sizeof(s_kbooltests) / sizeof(s_kbooltests[0]); }
#endif
