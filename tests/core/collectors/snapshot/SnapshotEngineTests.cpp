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

#include "SnapshotEngine.hpp"

using namespace monix::collectors::snapshot;

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

static bool test_snapshot_kind_names() {
  ASSERT_EQ(std::string(SnapshotKindName(SnapshotKind::Initial)), "Initial");
  ASSERT_EQ(std::string(SnapshotKindName(SnapshotKind::Recovery)), "Recovery");
  ASSERT_EQ(std::string(SnapshotKindName(SnapshotKind::Manual)), "Manual");
  return true;
}

static bool test_snapshot_state_names() {
  ASSERT_EQ(std::string(SnapshotStateName(SnapshotState::Pending)), "Pending");
  ASSERT_EQ(std::string(SnapshotStateName(SnapshotState::Running)), "Running");
  ASSERT_EQ(std::string(SnapshotStateName(SnapshotState::Completed)), "Completed");
  ASSERT_EQ(std::string(SnapshotStateName(SnapshotState::Failed)), "Failed");
  ASSERT_EQ(std::string(SnapshotStateName(SnapshotState::Partial)), "Partial");
  return true;
}

static bool test_event_kind_names() {
  ASSERT_EQ(std::string(SnapshotEventKindName(SnapshotEventKind::Started)), "Started");
  ASSERT_EQ(std::string(SnapshotEventKindName(SnapshotEventKind::Completed)), "Completed");
  ASSERT_EQ(std::string(SnapshotEventKindName(SnapshotEventKind::Failed)), "Failed");
  return true;
}

static bool test_event_kind_actions() {
  ASSERT_EQ(SnapshotEventKindAction(SnapshotEventKind::Started), "snapshot.started");
  ASSERT_EQ(SnapshotEventKindAction(SnapshotEventKind::Completed), "snapshot.completed");
  ASSERT_EQ(SnapshotEventKindAction(SnapshotEventKind::Failed), "snapshot.failed");
  return true;
}

static bool test_observation_origin_names() {
  ASSERT_EQ(std::string(ObservationOriginName(ObservationOrigin::InitialSnapshot)), "InitialSnapshot");
  ASSERT_EQ(std::string(ObservationOriginName(ObservationOrigin::RecoverySnapshot)), "RecoverySnapshot");
  ASSERT_EQ(std::string(ObservationOriginName(ObservationOrigin::ManualSnapshot)), "ManualSnapshot");
  ASSERT_EQ(std::string(ObservationOriginName(ObservationOrigin::Polling)), "Polling");
  return true;
}

static bool test_meta_validity() {
  SnapshotMeta meta;
  ASSERT_FALSE(meta.isValid());
  meta.collector_name = "ProcessCollector";
  ASSERT_TRUE(meta.isValid());
  return true;
}

static bool test_meta_state_checks() {
  SnapshotMeta meta;
  meta.state = SnapshotState::Completed;
  ASSERT_TRUE(meta.isComplete());
  ASSERT_FALSE(meta.isFailed());
  ASSERT_FALSE(meta.isPartial());

  meta.state = SnapshotState::Failed;
  ASSERT_FALSE(meta.isComplete());
  ASSERT_TRUE(meta.isFailed());

  meta.state = SnapshotState::Partial;
  ASSERT_TRUE(meta.isPartial());
  return true;
}

static bool test_meta_progress() {
  SnapshotMeta meta;
  meta.items_expected = 100;
  meta.items_captured = 50;
  ASSERT_EQ(meta.progress(), 0.5);
  meta.items_captured = 100;
  ASSERT_EQ(meta.progress(), 1.0);
  meta.items_expected = 0;
  ASSERT_EQ(meta.progress(), 0.0);
  return true;
}

static bool test_meta_origin() {
  SnapshotMeta meta;
  meta.kind = SnapshotKind::Initial;
  ASSERT_EQ(meta.origin(), ObservationOrigin::InitialSnapshot);
  meta.kind = SnapshotKind::Recovery;
  ASSERT_EQ(meta.origin(), ObservationOrigin::RecoverySnapshot);
  meta.kind = SnapshotKind::Manual;
  ASSERT_EQ(meta.origin(), ObservationOrigin::ManualSnapshot);
  return true;
}

static bool test_meta_summary() {
  SnapshotMeta meta;
  meta.collector_name = "Test";
  meta.kind = SnapshotKind::Manual;
  meta.state = SnapshotState::Completed;
  meta.items_captured = 10;
  meta.items_expected = 10;
  std::string s = meta.summary();
  ASSERT_TRUE(s.find("Test") != std::string::npos);
  ASSERT_TRUE(s.find("Manual") != std::string::npos);
  ASSERT_TRUE(s.find("Completed") != std::string::npos);
  return true;
}

static bool test_event_validity() {
  SnapshotEvent event;
  ASSERT_FALSE(event.isValid());
  event.collector_name = "Test";
  ASSERT_TRUE(event.isValid());
  return true;
}

static bool test_event_summary() {
  SnapshotEvent event;
  event.collector_name = "Test";
  event.event_kind = SnapshotEventKind::Started;
  event.snapshot_kind = SnapshotKind::Manual;
  std::string s = event.summary();
  ASSERT_TRUE(s.find("Started") != std::string::npos);
  ASSERT_TRUE(s.find("Test") != std::string::npos);
  return true;
}

static bool test_begin_snapshot() {
  SnapshotEngine engine;
  SnapshotId id = engine.beginSnapshot("ProcessCollector", SnapshotKind::Initial, 100);
  ASSERT_TRUE(id > 0);
  ASSERT_TRUE(engine.isActive(id));
  ASSERT_EQ(engine.activeCount(), static_cast<std::size_t>(1));
  ASSERT_EQ(engine.totalSnapshots(), static_cast<std::size_t>(1));
  return true;
}

static bool test_complete_snapshot() {
  SnapshotEngine engine;
  SnapshotId id = engine.beginSnapshot("ProcessCollector", SnapshotKind::Initial, 100);
  engine.updateProgress(id, 100);
  ASSERT_TRUE(engine.completeSnapshot(id));
  ASSERT_FALSE(engine.isActive(id));
  SnapshotMeta meta = engine.getSnapshot(id);
  ASSERT_TRUE(meta.isComplete());
  ASSERT_EQ(meta.items_captured, static_cast<std::size_t>(100));
  return true;
}

static bool test_fail_snapshot() {
  SnapshotEngine engine;
  SnapshotId id = engine.beginSnapshot("ProcessCollector", SnapshotKind::Recovery, 100);
  ASSERT_TRUE(engine.failSnapshot(id, "Access denied"));
  ASSERT_FALSE(engine.isActive(id));
  SnapshotMeta meta = engine.getSnapshot(id);
  ASSERT_TRUE(meta.isFailed());
  ASSERT_EQ(meta.error_message, "Access denied");
  return true;
}

static bool test_partial_snapshot() {
  SnapshotEngine engine;
  SnapshotId id = engine.beginSnapshot("DriverCollector", SnapshotKind::Recovery, 100);
  engine.updateProgress(id, 75, 25);
  ASSERT_TRUE(engine.completeSnapshot(id));
  SnapshotMeta meta = engine.getSnapshot(id);
  ASSERT_TRUE(meta.isPartial());
  ASSERT_EQ(meta.items_captured, static_cast<std::size_t>(75));
  ASSERT_EQ(meta.items_failed, static_cast<std::size_t>(25));
  return true;
}

static bool test_update_progress() {
  SnapshotEngine engine;
  SnapshotId id = engine.beginSnapshot("ServiceCollector", SnapshotKind::Manual, 50);
  engine.updateProgress(id, 25);
  SnapshotMeta meta = engine.getSnapshot(id);
  ASSERT_EQ(meta.items_captured, static_cast<std::size_t>(25));
  engine.updateProgress(id, 50, 3);
  meta = engine.getSnapshot(id);
  ASSERT_EQ(meta.items_captured, static_cast<std::size_t>(50));
  ASSERT_EQ(meta.items_failed, static_cast<std::size_t>(3));
  return true;
}

static bool test_callback_started() {
  SnapshotEngine engine;
  bool got_started = false;
  engine.setCallback([&](const SnapshotEvent& e) {
    if (e.event_kind == SnapshotEventKind::Started) got_started = true;
  });
  engine.beginSnapshot("Test", SnapshotKind::Manual);
  ASSERT_TRUE(got_started);
  return true;
}

static bool test_callback_completed() {
  SnapshotEngine engine;
  bool got_completed = false;
  engine.setCallback([&](const SnapshotEvent& e) {
    if (e.event_kind == SnapshotEventKind::Completed) got_completed = true;
  });
  SnapshotId id = engine.beginSnapshot("Test", SnapshotKind::Manual);
  engine.completeSnapshot(id);
  ASSERT_TRUE(got_completed);
  return true;
}

static bool test_callback_failed() {
  SnapshotEngine engine;
  bool got_failed = false;
  engine.setCallback([&](const SnapshotEvent& e) {
    if (e.event_kind == SnapshotEventKind::Failed) got_failed = true;
  });
  SnapshotId id = engine.beginSnapshot("Test", SnapshotKind::Manual);
  engine.failSnapshot(id, "error");
  ASSERT_TRUE(got_failed);
  return true;
}

static bool test_events_emitted() {
  SnapshotEngine engine;
  engine.setCallback([](const SnapshotEvent&) {});
  SnapshotId id1 = engine.beginSnapshot("A", SnapshotKind::Manual);
  engine.completeSnapshot(id1);
  SnapshotId id2 = engine.beginSnapshot("B", SnapshotKind::Initial);
  engine.failSnapshot(id2, "err");
  ASSERT_EQ(engine.eventsEmitted(), static_cast<std::size_t>(4));
  return true;
}

static bool test_active_snapshots() {
  SnapshotEngine engine;
  engine.setCallback([](const SnapshotEvent&) {});
  SnapshotId id1 = engine.beginSnapshot("A", SnapshotKind::Manual);
  SnapshotId id2 = engine.beginSnapshot("B", SnapshotKind::Initial);
  SnapshotId id3 = engine.beginSnapshot("C", SnapshotKind::Recovery);
  engine.completeSnapshot(id2);

  auto active = engine.activeSnapshots();
  ASSERT_EQ(active.size(), static_cast<std::size_t>(2));
  return true;
}

static bool test_completed_snapshots() {
  SnapshotEngine engine;
  engine.setCallback([](const SnapshotEvent&) {});
  SnapshotId id1 = engine.beginSnapshot("A", SnapshotKind::Manual);
  SnapshotId id2 = engine.beginSnapshot("B", SnapshotKind::Initial);
  engine.completeSnapshot(id1);
  engine.failSnapshot(id2, "err");

  auto completed = engine.completedSnapshots();
  ASSERT_EQ(completed.size(), static_cast<std::size_t>(1));
  return true;
}

static bool test_large_snapshot() {
  SnapshotEngine engine;
  engine.setCallback([](const SnapshotEvent&) {});
  SnapshotId id = engine.beginSnapshot("LargeCollector", SnapshotKind::Initial, 10000);
  for (std::size_t i = 0; i < 10000; i += 100) {
    engine.updateProgress(id, i);
  }
  engine.updateProgress(id, 10000);
  ASSERT_TRUE(engine.completeSnapshot(id));
  SnapshotMeta meta = engine.getSnapshot(id);
  ASSERT_TRUE(meta.isComplete());
  ASSERT_EQ(meta.progress(), 1.0);
  return true;
}

static bool test_invalid_operations() {
  SnapshotEngine engine;
  engine.setCallback([](const SnapshotEvent&) {});
  SnapshotId id = engine.beginSnapshot("Test", SnapshotKind::Manual);
  engine.completeSnapshot(id);
  ASSERT_FALSE(engine.completeSnapshot(id));
  ASSERT_FALSE(engine.failSnapshot(id, "already done"));
  ASSERT_FALSE(engine.completeSnapshot(9999));
  ASSERT_FALSE(engine.failSnapshot(9999));
  return true;
}

static bool test_no_callback_no_crash() {
  SnapshotEngine engine;
  SnapshotId id = engine.beginSnapshot("Test", SnapshotKind::Manual);
  engine.completeSnapshot(id);
  ASSERT_TRUE(true);
  return true;
}

static bool test_snapshot_not_realtime() {
  SnapshotEngine engine;
  SnapshotId id = engine.beginSnapshot("Test", SnapshotKind::Initial, 50);
  SnapshotMeta meta = engine.getSnapshot(id);
  ObservationOrigin origin = meta.origin();
  ASSERT_TRUE(origin == ObservationOrigin::InitialSnapshot ||
              origin == ObservationOrigin::RecoverySnapshot ||
              origin == ObservationOrigin::ManualSnapshot);
  ASSERT_TRUE(origin != ObservationOrigin::Polling);
  return true;
}

#ifndef MONIX_KERNEL_BUILD
int main() {
  std::cerr << "=== MONIX Snapshot Engine Tests ===\n\n";

  RUN_TEST(snapshot_kind_names);
  RUN_TEST(snapshot_state_names);
  RUN_TEST(event_kind_names);
  RUN_TEST(event_kind_actions);
  RUN_TEST(observation_origin_names);
  RUN_TEST(meta_validity);
  RUN_TEST(meta_state_checks);
  RUN_TEST(meta_progress);
  RUN_TEST(meta_origin);
  RUN_TEST(meta_summary);
  RUN_TEST(event_validity);
  RUN_TEST(event_summary);
  RUN_TEST(begin_snapshot);
  RUN_TEST(complete_snapshot);
  RUN_TEST(fail_snapshot);
  RUN_TEST(partial_snapshot);
  RUN_TEST(update_progress);
  RUN_TEST(callback_started);
  RUN_TEST(callback_completed);
  RUN_TEST(callback_failed);
  RUN_TEST(events_emitted);
  RUN_TEST(active_snapshots);
  RUN_TEST(completed_snapshots);
  RUN_TEST(large_snapshot);
  RUN_TEST(invalid_operations);
  RUN_TEST(no_callback_no_crash);
  RUN_TEST(snapshot_not_realtime);

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
  {"Snapshot kind names", test_snapshot_kind_names},
  {"Snapshot state names", test_snapshot_state_names},
  {"Event kind names", test_event_kind_names},
  {"Event kind actions", test_event_kind_actions},
  {"Observation origin names", test_observation_origin_names},
  {"Meta validity", test_meta_validity},
  {"Meta state checks", test_meta_state_checks},
  {"Meta progress", test_meta_progress},
  {"Meta origin", test_meta_origin},
  {"Meta summary", test_meta_summary},
  {"Event validity", test_event_validity},
  {"Event summary", test_event_summary},
  {"Begin snapshot", test_begin_snapshot},
  {"Complete snapshot", test_complete_snapshot},
  {"Fail snapshot", test_fail_snapshot},
  {"Partial snapshot", test_partial_snapshot},
  {"Update progress", test_update_progress},
  {"Callback started", test_callback_started},
  {"Callback completed", test_callback_completed},
  {"Callback failed", test_callback_failed},
  {"Events emitted", test_events_emitted},
  {"Active snapshots", test_active_snapshots},
  {"Completed snapshots", test_completed_snapshots},
  {"Large snapshot", test_large_snapshot},
  {"Invalid operations", test_invalid_operations},
  {"No callback no crash", test_no_callback_no_crash},
  {"Snapshot not realtime", test_snapshot_not_realtime},
};

const KBoolTestEntry* GetKBoolTests_SnapshotEngine() { return s_kbooltests; }
std::size_t GetKBoolTestCount_SnapshotEngine() { return sizeof(s_kbooltests) / sizeof(s_kbooltests[0]); }
#endif
