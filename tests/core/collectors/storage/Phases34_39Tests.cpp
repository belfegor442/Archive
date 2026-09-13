#include <cassert>
#include <chrono>
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <unordered_map>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "EventStorage.hpp"
#include "RetentionPolicy.hpp"
#include "IntegrityStorage.hpp"
#include "LiveStream.hpp"
#include "EventInspector.hpp"
#include "ActivityView.hpp"

using namespace monix::collectors::eventstore;
using namespace monix::collectors::retention;
using namespace monix::collectors::integritystore;
using namespace monix::collectors::livestream;
using namespace monix::collectors::inspector;
using namespace monix::collectors::activityview;

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

static StoredEvent makeEvent(const std::string& id, const std::string& type, EventSeverity sev = EventSeverity::Info) {
  StoredEvent e;
  e.event_id = id;
  e.event_type = type;
  e.severity = sev;
  e.timestamp_ms = 1000;
  e.source = "test";
  return e;
}

// ===== Event Storage Tests =====

static bool test_es_insert() {
  EventStorage storage;
  auto e = makeEvent("E1", "file.modified");
  storage.insert(e);
  ASSERT_EQ(storage.count(), static_cast<std::size_t>(1));
  return true;
}

static bool test_es_query() {
  EventStorage storage;
  storage.insert(makeEvent("E1", "file.modified"));
  storage.insert(makeEvent("E2", "process.start"));
  storage.insert(makeEvent("E3", "file.modified"));

  QueryFilter filter;
  filter.event_type = "file.modified";
  auto results = storage.query(filter);
  ASSERT_EQ(results.size(), static_cast<std::size_t>(2));
  return true;
}

static bool test_es_large_volume() {
  StorageConfig cfg;
  cfg.max_events = 10000;
  cfg.rotation_size = 5000;
  EventStorage storage(cfg);
  storage.insert(makeEvent("header", "init"));
  for (int i = 0; i < 10000; i++) {
    storage.insert(makeEvent("E" + std::to_string(i), "event." + std::to_string(i % 10)));
  }
  ASSERT_TRUE(storage.count() <= static_cast<std::size_t>(10000));
  return true;
}

static bool test_es_rotation() {
  StorageConfig cfg;
  cfg.max_events = 100;
  cfg.rotation_size = 50;
  EventStorage storage(cfg);
  for (int i = 0; i < 150; i++) {
    storage.insert(makeEvent("E" + std::to_string(i), "event"));
  }
  ASSERT_TRUE(storage.count() <= static_cast<std::size_t>(100));
  return true;
}

static bool test_es_retention() {
  EventStorage storage;
  storage.insert(makeEvent("E1", "debug.event", EventSeverity::Debug));
  storage.insert(makeEvent("E2", "critical.event", EventSeverity::Critical));

  QueryFilter filter;
  filter.min_severity = EventSeverity::Critical;
  filter.max_severity = EventSeverity::Critical;
  storage.retain(filter);
  ASSERT_EQ(storage.count(), static_cast<std::size_t>(1));
  return true;
}

static bool test_es_corruption() {
  EventStorage storage;
  storage.insert(makeEvent("E1", "test"));
  auto retrieved = storage.getById("E1");
  ASSERT_TRUE(retrieved.isValid());
  ASSERT_EQ(retrieved.event_id, "E1");
  auto missing = storage.getById("nonexistent");
  ASSERT_FALSE(missing.isValid());
  return true;
}

static bool test_es_restart_recovery() {
  EventStorage storage;
  for (int i = 0; i < 50; i++) {
    storage.insert(makeEvent("E" + std::to_string(i), "event"));
  }
  ASSERT_EQ(storage.count(), static_cast<std::size_t>(50));
  return true;
}

static bool test_es_batch_insert() {
  EventStorage storage;
  std::vector<StoredEvent> batch;
  for (int i = 0; i < 100; i++) {
    batch.push_back(makeEvent("B" + std::to_string(i), "batch"));
  }
  storage.insertBatch(batch);
  ASSERT_EQ(storage.count(), static_cast<std::size_t>(100));
  return true;
}

static bool test_es_correlation_search() {
  EventStorage storage;
  auto e1 = makeEvent("E1", "login");
  e1.correlation_id = "C1";
  auto e2 = makeEvent("E2", "process");
  e2.correlation_id = "C1";
  auto e3 = makeEvent("E3", "other");
  e3.correlation_id = "C2";
  storage.insert(e1);
  storage.insert(e2);
  storage.insert(e3);

  auto results = storage.byCorrelationId("C1");
  ASSERT_EQ(results.size(), static_cast<std::size_t>(2));
  return true;
}

// ===== Retention Policy Tests =====

static bool test_ret_mode_names() {
  ASSERT_EQ(std::string(RetentionModeName(RetentionMode::Delete)), "Delete");
  ASSERT_EQ(std::string(RetentionModeName(RetentionMode::Archive)), "Archive");
  ASSERT_EQ(std::string(RetentionModeName(RetentionMode::Compact)), "Compact");
  return true;
}

static bool test_ret_add_rule() {
  RetentionPolicy policy;
  RetentionRule rule;
  rule.name = "debug_7d";
  rule.max_age_ms = 7 * 24 * 3600000;
  rule.min_severity = EventSeverity::Debug;
  rule.max_severity = EventSeverity::Debug;
  policy.addRule(rule);
  ASSERT_EQ(policy.ruleCount(), static_cast<std::size_t>(1));
  return true;
}

static bool test_ret_remove_rule() {
  RetentionPolicy policy;
  RetentionRule r1; r1.name = "r1"; r1.mode = RetentionMode::Delete; r1.max_age_ms = 1000;
  r1.min_severity = EventSeverity::Debug; r1.max_severity = EventSeverity::Debug; r1.enabled = true;
  policy.addRule(r1);
  RetentionRule r2; r2.name = "r2"; r2.mode = RetentionMode::Delete; r2.max_age_ms = 2000;
  r2.min_severity = EventSeverity::Info; r2.max_severity = EventSeverity::Info; r2.enabled = true;
  policy.addRule(r2);
  ASSERT_TRUE(policy.removeRule("r1"));
  ASSERT_EQ(policy.ruleCount(), static_cast<std::size_t>(1));
  return true;
}

static bool test_ret_enforce() {
  EventStorage storage;
  storage.insert(makeEvent("E1", "debug", EventSeverity::Debug));
  storage.insert(makeEvent("E2", "critical", EventSeverity::Critical));

  RetentionPolicy policy;
  RetentionConfig cfg;
  cfg.global_max_age_ms = 1;
  cfg.global_max_events = 1000000;
  cfg.enabled = true;
  policy.setConfig(cfg);

  RetentionRule rule;
  rule.name = "old_debug";
  rule.max_age_ms = 1;
  rule.min_severity = EventSeverity::Debug;
  rule.max_severity = EventSeverity::Debug;
  rule.enabled = true;
  policy.addRule(rule);

  Sleep(10);
  auto result = policy.enforce(storage);
  ASSERT_TRUE(result.rules_applied >= static_cast<std::size_t>(1));
  return true;
}

// ===== Integrity Storage Tests =====

static bool test_int_status_names() {
  ASSERT_EQ(std::string(IntegrityStatusName(IntegrityStatus::Verified)), "Verified");
  ASSERT_EQ(std::string(IntegrityStatusName(IntegrityStatus::Corrupted)), "Corrupted");
  ASSERT_EQ(std::string(IntegrityStatusName(IntegrityStatus::Missing)), "Missing");
  return true;
}

static bool test_int_store_and_verify() {
  IntegrityStorage storage;
  IntegrityRecord record;
  record.event_id = "E1";
  record.hash = "abc123";
  record.status = IntegrityStatus::Unchecked;
  storage.storeRecord(record);

  ASSERT_TRUE(storage.verifyEvent("E1", "abc123"));
  ASSERT_EQ(storage.getStatus("E1"), IntegrityStatus::Verified);
  return true;
}

static bool test_int_detect_corruption() {
  IntegrityStorage storage;
  IntegrityRecord record;
  record.event_id = "E1";
  record.hash = "abc123";
  storage.storeRecord(record);

  ASSERT_FALSE(storage.verifyEvent("E1", "wrong_hash"));
  ASSERT_EQ(storage.getStatus("E1"), IntegrityStatus::Corrupted);
  return true;
}

static bool test_int_corrupted_list() {
  IntegrityStorage storage;
  IntegrityRecord r1; r1.event_id = "E1"; r1.hash = "a";
  IntegrityRecord r2; r2.event_id = "E2"; r2.hash = "b";
  storage.storeRecord(r1);
  storage.storeRecord(r2);
  storage.verifyEvent("E1", "wrong");
  storage.verifyEvent("E2", "b");

  auto corrupted = storage.corruptedRecords();
  ASSERT_EQ(corrupted.size(), static_cast<std::size_t>(1));
  return true;
}

static bool test_int_verify_all() {
  IntegrityStorage storage;
  storage.storeRecord({"E1", "hash1", "", 0, IntegrityStatus::Unchecked});
  storage.storeRecord({"E2", "hash2", "", 0, IntegrityStatus::Unchecked});

  std::unordered_map<std::string, std::string> current = {{"E1", "hash1"}, {"E2", "wrong"}};
  auto result = storage.verifyAll(current);
  ASSERT_TRUE(result.verified >= static_cast<std::size_t>(1));
  ASSERT_TRUE(result.corrupted >= static_cast<std::size_t>(1));
  return true;
}

// ===== Live Stream Tests =====

static bool test_ls_state_names() {
  ASSERT_EQ(std::string(StreamStateName(StreamState::Paused)), "Paused");
  ASSERT_EQ(std::string(StreamStateName(StreamState::Running)), "Running");
  ASSERT_EQ(std::string(StreamStateName(StreamState::Following)), "Following");
  return true;
}

static bool test_ls_push_event() {
  LiveStream stream;
  stream.setCallback([](const StreamEvent&) {});
  StreamEvent e;
  e.event_id = "E1";
  e.event_type = "test";
  e.summary = "test event";
  stream.pushEvent(e);
  ASSERT_EQ(stream.totalEvents(), static_cast<std::size_t>(1));
  return true;
}

static bool test_ls_pause_resume() {
  LiveStream stream;
  stream.pause();
  ASSERT_TRUE(stream.isPaused());
  stream.resume();
  ASSERT_FALSE(stream.isPaused());
  return true;
}

static bool test_ls_filter() {
  LiveStream stream;
  stream.setCallback([](const StreamEvent&) {});
  StreamEvent e1; e1.event_id = "E1"; e1.event_type = "file"; e1.summary = "file mod";
  StreamEvent e2; e2.event_id = "E2"; e2.event_type = "process"; e2.summary = "proc start";
  stream.pushEvent(e1);
  stream.pushEvent(e2);

  StreamFilter filter;
  filter.event_type = "file";
  stream.setFilter(filter);
  ASSERT_EQ(stream.visibleCount(), static_cast<std::size_t>(1));

  stream.clearFilter();
  ASSERT_EQ(stream.visibleCount(), static_cast<std::size_t>(2));
  return true;
}

static bool test_ls_search() {
  LiveStream stream;
  StreamEvent e1; e1.event_id = "E1"; e1.summary = "file modified";
  StreamEvent e2; e2.event_id = "E2"; e2.summary = "process started";
  stream.pushEvent(e1);
  stream.pushEvent(e2);

  StreamFilter filter;
  filter.search_text = "file";
  stream.setFilter(filter);
  ASSERT_EQ(stream.visibleCount(), static_cast<std::size_t>(1));
  return true;
}

static bool test_ls_follow() {
  LiveStream stream;
  stream.follow();
  ASSERT_TRUE(stream.isFollowing());
  return true;
}

static bool test_ls_expand_collapse() {
  LiveStream stream;
  stream.setCallback([](const StreamEvent&) {});
  StreamEvent e; e.event_id = "E1"; e.summary = "test";
  stream.pushEvent(e);

  stream.expand("E1");
  auto visible = stream.visibleEvents();
  ASSERT_TRUE(visible[0].expanded);

  stream.collapse("E1");
  visible = stream.visibleEvents();
  ASSERT_FALSE(visible[0].expanded);
  return true;
}

// ===== Event Inspector Tests =====

static bool test_ins_inspect() {
  EventInspector inspector;
  EventDetail detail;
  detail.event_id = "E1";
  detail.event_type = "file.modified";
  detail.severity = "Info";
  detail.source = "filesystem";
  inspector.inspect(detail);

  ASSERT_TRUE(inspector.hasEvent());
  ASSERT_EQ(inspector.currentEvent().event_id, "E1");
  return true;
}

static bool test_ins_navigation_links() {
  EventInspector inspector;
  EventDetail detail;
  detail.event_id = "E2";
  detail.parent_event_id = "E1";
  detail.activity_id = "A1";
  detail.actor = "proc1";
  detail.session_id = "S1";
  detail.source = "dev1";
  inspector.inspect(detail);

  auto links = inspector.navigationLinks();
  ASSERT_TRUE(links.size() >= static_cast<std::size_t>(4));
  return true;
}

static bool test_ins_parent_link() {
  EventInspector inspector;
  EventDetail detail;
  detail.event_id = "E2";
  detail.parent_event_id = "E1";
  inspector.inspect(detail);

  auto parents = inspector.parentLinks();
  ASSERT_EQ(parents.size(), static_cast<std::size_t>(1));
  ASSERT_EQ(parents[0].event_id, "E1");
  return true;
}

static bool test_ins_child_links() {
  EventInspector inspector;
  EventDetail detail;
  detail.event_id = "E1";
  inspector.setChildren("E1", {"E2", "E3"});
  inspector.inspect(detail);

  auto children = inspector.childLinks();
  ASSERT_EQ(children.size(), static_cast<std::size_t>(2));
  return true;
}

static bool test_ins_clear() {
  EventInspector inspector;
  EventDetail detail;
  detail.event_id = "E1";
  inspector.inspect(detail);
  ASSERT_TRUE(inspector.hasEvent());
  inspector.clear();
  ASSERT_FALSE(inspector.hasEvent());
  return true;
}

// ===== Activity View Tests =====

static bool test_av_add_activity() {
  ActivityView view;
  ActivityTimeline activity;
  activity.activity_id = "A1";
  activity.name = "Login Activity";
  view.addActivity(activity);
  ASSERT_EQ(view.activityCount(), static_cast<std::size_t>(1));
  return true;
}

static bool test_av_add_event() {
  ActivityView view;
  ActivityEvent event;
  event.event_id = "E1";
  event.timestamp_ms = 1000;
  event.event_type = "login";
  event.summary = "user login";
  view.addEvent("A1", event);
  ASSERT_TRUE(view.hasActivity("A1"));
  auto activity = view.getActivity("A1");
  ASSERT_EQ(activity.eventCount(), static_cast<std::size_t>(1));
  return true;
}

static bool test_av_timeline() {
  ActivityView view;
  ActivityEvent e1; e1.event_id = "E1"; e1.timestamp_ms = 1000; e1.event_type = "login"; e1.summary = "user login";
  ActivityEvent e2; e2.event_id = "E2"; e2.timestamp_ms = 2000; e2.event_type = "process"; e2.summary = "start cmd";
  ActivityEvent e3; e3.event_id = "E3"; e3.timestamp_ms = 3000; e3.event_type = "file"; e3.summary = "modified";

  view.addEvent("A1", e1);
  view.addEvent("A1", e2);
  view.addEvent("A1", e3);

  auto activity = view.getActivity("A1");
  ASSERT_EQ(activity.eventCount(), static_cast<std::size_t>(3));
  ASSERT_TRUE(activity.events[0].timestamp_ms <= activity.events[1].timestamp_ms);
  return true;
}

static bool test_av_render() {
  ActivityView view;
  ActivityEvent e; e.event_id = "E1"; e.timestamp_ms = 60000; e.event_type = "login"; e.summary = "user login";
  view.addEvent("A1", e);

  auto rendered = view.renderTimeline("A1");
  ASSERT_TRUE(rendered.find("A1") != std::string::npos);
  ASSERT_TRUE(rendered.find("login") != std::string::npos);
  return true;
}

static bool test_av_active_completed() {
  ActivityView view;
  ActivityTimeline active;
  active.activity_id = "A1";
  active.ended_ms = 0;
  view.addActivity(active);

  ActivityTimeline completed;
  completed.activity_id = "A2";
  completed.started_ms = 1000;
  completed.ended_ms = 2000;
  view.addActivity(completed);

  ASSERT_EQ(view.activeActivities().size(), static_cast<std::size_t>(1));
  ASSERT_EQ(view.completedActivities().size(), static_cast<std::size_t>(1));
  return true;
}

static bool test_av_duration() {
  ActivityTimeline activity;
  activity.activity_id = "A1";
  activity.started_ms = 1000;
  activity.ended_ms = 5000;
  ASSERT_EQ(activity.durationMs(), static_cast<std::int64_t>(4000));
  return true;
}

#ifndef MONIX_KERNEL_BUILD
int main() {
  std::cerr << "=== MONIX Phases 34-39 Tests ===\n\n";

  RUN_TEST(es_insert);
  RUN_TEST(es_query);
  RUN_TEST(es_large_volume);
  RUN_TEST(es_rotation);
  RUN_TEST(es_retention);
  RUN_TEST(es_corruption);
  RUN_TEST(es_restart_recovery);
  RUN_TEST(es_batch_insert);
  RUN_TEST(es_correlation_search);
  RUN_TEST(ret_mode_names);
  RUN_TEST(ret_add_rule);
  RUN_TEST(ret_remove_rule);
  RUN_TEST(ret_enforce);
  RUN_TEST(int_status_names);
  RUN_TEST(int_store_and_verify);
  RUN_TEST(int_detect_corruption);
  RUN_TEST(int_corrupted_list);
  RUN_TEST(int_verify_all);
  RUN_TEST(ls_state_names);
  RUN_TEST(ls_push_event);
  RUN_TEST(ls_pause_resume);
  RUN_TEST(ls_filter);
  RUN_TEST(ls_search);
  RUN_TEST(ls_follow);
  RUN_TEST(ls_expand_collapse);
  RUN_TEST(ins_inspect);
  RUN_TEST(ins_navigation_links);
  RUN_TEST(ins_parent_link);
  RUN_TEST(ins_child_links);
  RUN_TEST(ins_clear);
  RUN_TEST(av_add_activity);
  RUN_TEST(av_add_event);
  RUN_TEST(av_timeline);
  RUN_TEST(av_render);
  RUN_TEST(av_active_completed);
  RUN_TEST(av_duration);

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
  {"Event store insert", test_es_insert},
  {"Event store query", test_es_query},
  {"Event store large volume", test_es_large_volume},
  {"Event store rotation", test_es_rotation},
  {"Event store retention", test_es_retention},
  {"Event store corruption", test_es_corruption},
  {"Event store restart recovery", test_es_restart_recovery},
  {"Event store batch insert", test_es_batch_insert},
  {"Event store correlation search", test_es_correlation_search},
  {"Retention mode names", test_ret_mode_names},
  {"Retention add rule", test_ret_add_rule},
  {"Retention remove rule", test_ret_remove_rule},
  {"Retention enforce", test_ret_enforce},
  {"Integrity status names", test_int_status_names},
  {"Integrity store and verify", test_int_store_and_verify},
  {"Integrity detect corruption", test_int_detect_corruption},
  {"Integrity corrupted list", test_int_corrupted_list},
  {"Integrity verify all", test_int_verify_all},
  {"Live stream state names", test_ls_state_names},
  {"Live stream push event", test_ls_push_event},
  {"Live stream pause resume", test_ls_pause_resume},
  {"Live stream filter", test_ls_filter},
  {"Live stream search", test_ls_search},
  {"Live stream follow", test_ls_follow},
  {"Live stream expand collapse", test_ls_expand_collapse},
  {"Inspector inspect", test_ins_inspect},
  {"Inspector navigation links", test_ins_navigation_links},
  {"Inspector parent link", test_ins_parent_link},
  {"Inspector child links", test_ins_child_links},
  {"Inspector clear", test_ins_clear},
  {"Activity view add activity", test_av_add_activity},
  {"Activity view add event", test_av_add_event},
  {"Activity view timeline", test_av_timeline},
  {"Activity view render", test_av_render},
  {"Activity view active completed", test_av_active_completed},
  {"Activity view duration", test_av_duration},
};

const KBoolTestEntry* GetKBoolTests_Phases34_39() { return s_kbooltests; }
std::size_t GetKBoolTestCount_Phases34_39() { return sizeof(s_kbooltests) / sizeof(s_kbooltests[0]); }
#endif
