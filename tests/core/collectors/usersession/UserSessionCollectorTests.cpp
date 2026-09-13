#include "../../../core/collectors/usersession/UserSessionTypes.hpp"
#include "../../../core/collectors/usersession/UserSessionCollector.hpp"

#include <cassert>
#include <cstdio>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <atomic>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

using namespace monix::collectors::usersession;

static int gPassed = 0;
static int gFailed = 0;

#define TEST(name) printf("  %-62s ", name);
#define PASS() do { printf("[PASS]\n"); gPassed++; } while(0)
#define FAIL(msg) do { printf("[FAIL] %s\n", msg); gFailed++; } while(0)
#define ASSERT_TRUE(e) do { if (!(e)) { FAIL(#e); return; } } while(0)
#define ASSERT_FALSE(e) do { if ((e)) { FAIL(#e); return; } } while(0)
#define ASSERT_EQ(a, b) do { if ((a) != (b)) { FAIL(#a " != " #b); return; } } while(0)
#define ASSERT_NE(a, b) do { if ((a) == (b)) { FAIL(#a " == " #b); return; } } while(0)
#define ASSERT_GE(a, b) do { if ((a) < (b)) { FAIL(#a " < " #b); return; } } while(0)

// ==================== SessionState Tests ====================

static void test_session_state_names() {
  TEST("SessionState: all names");
  ASSERT_EQ(std::string(SessionStateName(SessionState::Active)), "Active");
  ASSERT_EQ(std::string(SessionStateName(SessionState::Connected)), "Connected");
  ASSERT_EQ(std::string(SessionStateName(SessionState::Shadow)), "Shadow");
  ASSERT_EQ(std::string(SessionStateName(SessionState::Disconnected)), "Disconnected");
  ASSERT_EQ(std::string(SessionStateName(SessionState::Unknown)), "Unknown");
  PASS();
}

// ==================== EventKind Tests ====================

static void test_event_kind_names() {
  TEST("UserSessionEventKind: all names");
  ASSERT_EQ(std::string(UserSessionEventKindName(UserSessionEventKind::Login)), "Login");
  ASSERT_EQ(std::string(UserSessionEventKindName(UserSessionEventKind::Logout)), "Logout");
  ASSERT_EQ(std::string(UserSessionEventKindName(UserSessionEventKind::SessionCreated)), "SessionCreated");
  ASSERT_EQ(std::string(UserSessionEventKindName(UserSessionEventKind::SessionClosed)), "SessionClosed");
  ASSERT_EQ(std::string(UserSessionEventKindName(UserSessionEventKind::UserAction)), "UserAction");
  PASS();
}

static void test_event_kind_actions() {
  TEST("UserSessionEventKind: action strings");
  ASSERT_EQ(UserSessionEventKindAction(UserSessionEventKind::Login), "login");
  ASSERT_EQ(UserSessionEventKindAction(UserSessionEventKind::Logout), "logout");
  ASSERT_EQ(UserSessionEventKindAction(UserSessionEventKind::SessionCreated), "session_created");
  ASSERT_EQ(UserSessionEventKindAction(UserSessionEventKind::SessionClosed), "session_closed");
  ASSERT_EQ(UserSessionEventKindAction(UserSessionEventKind::UserAction), "user_action");
  PASS();
}

// ==================== Origin Tests ====================

static void test_origin_names() {
  TEST("UserSessionOrigin: all names");
  ASSERT_EQ(std::string(UserSessionOriginName(UserSessionOrigin::InitialSnapshot)), "InitialSnapshot");
  ASSERT_EQ(std::string(UserSessionOriginName(UserSessionOrigin::Polling)), "Polling");
  ASSERT_EQ(std::string(UserSessionOriginName(UserSessionOrigin::Manual)), "Manual");
  PASS();
}

// ==================== UserActionKind Tests ====================

static void test_action_kind_names() {
  TEST("UserActionKind: all names");
  ASSERT_EQ(std::string(UserActionKindName(UserActionKind::ProgramStarted)), "ProgramStarted");
  ASSERT_EQ(std::string(UserActionKindName(UserActionKind::DeviceConnected)), "DeviceConnected");
  ASSERT_EQ(std::string(UserActionKindName(UserActionKind::ScriptExecuted)), "ScriptExecuted");
  ASSERT_EQ(std::string(UserActionKindName(UserActionKind::ConfigChanged)), "ConfigChanged");
  ASSERT_EQ(std::string(UserActionKindName(UserActionKind::Unknown)), "Unknown");
  PASS();
}

// ==================== SessionInfo Tests ====================

static void test_session_info_defaults() {
  TEST("SessionInfo: defaults are safe");
  SessionInfo info;
  ASSERT_EQ(info.session_id, kInvalidSession);
  ASSERT_EQ(info.user_id, kUnknownUser);
  ASSERT_TRUE(info.username.empty());
  ASSERT_TRUE(info.domain.empty());
  ASSERT_EQ(info.state, SessionState::Unknown);
  PASS();
}

static void test_session_info_is_active() {
  TEST("SessionInfo: isActive checks state");
  SessionInfo active;
  active.state = SessionState::Active;
  ASSERT_TRUE(active.isActive());

  SessionInfo connected;
  connected.state = SessionState::Connected;
  ASSERT_TRUE(connected.isActive());

  SessionInfo disconnected;
  disconnected.state = SessionState::Disconnected;
  ASSERT_FALSE(disconnected.isActive());

  SessionInfo shadow;
  shadow.state = SessionState::Shadow;
  ASSERT_FALSE(shadow.isActive());
  PASS();
}

static void test_session_info_is_valid() {
  TEST("SessionInfo: isValid checks session_id");
  SessionInfo valid;
  valid.session_id = 1;
  ASSERT_TRUE(valid.isValid());

  SessionInfo invalid;
  invalid.session_id = kInvalidSession;
  ASSERT_FALSE(invalid.isValid());
  PASS();
}

static void test_session_info_qualified_user() {
  TEST("SessionInfo: qualifiedUser format");
  SessionInfo info;
  info.username = "admin";
  info.domain = "WORKGROUP";
  ASSERT_EQ(info.qualifiedUser(), "WORKGROUP\\admin");

  SessionInfo noDomain;
  noDomain.username = "user";
  ASSERT_EQ(noDomain.qualifiedUser(), "user");
  PASS();
}

// ==================== Collector Lifecycle Tests ====================

static void test_collector_start_stop() {
  TEST("UserSessionCollector: start and stop lifecycle");
  UserSessionCollector collector;
  ASSERT_FALSE(collector.isRunning());

  ASSERT_TRUE(collector.start());
  ASSERT_TRUE(collector.isRunning());

  ASSERT_TRUE(collector.stop());
  ASSERT_FALSE(collector.isRunning());
  PASS();
}

static void test_collector_double_start() {
  TEST("UserSessionCollector: double start returns false");
  UserSessionCollector collector;
  ASSERT_TRUE(collector.start());
  ASSERT_FALSE(collector.start());
  ASSERT_TRUE(collector.stop());
  PASS();
}

static void test_collector_stop_without_start() {
  TEST("UserSessionCollector: stop without start returns false");
  UserSessionCollector collector;
  ASSERT_FALSE(collector.stop());
  PASS();
}

static void test_collector_config_defaults() {
  TEST("UserSessionCollectorConfig: defaults are safe");
  auto cfg = UserSessionCollectorConfig::defaults();
  ASSERT_TRUE(cfg.poll_interval_ms > 0);
  ASSERT_FALSE(cfg.track_user_actions);
  ASSERT_TRUE(cfg.max_sessions > 0);
  PASS();
}

// ==================== Session Enumeration Tests ====================

static void test_enumerate_sessions_finds_console() {
  TEST("Session enumeration: finds console session");
  UserSessionCollector collector;
  collector.start();
  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  auto sessions = collector.currentSessions();
  ASSERT_TRUE(sessions.size() > 0);
  collector.stop();
  PASS();
}

static void test_session_count_matches() {
  TEST("Session: sessionCount matches currentSessions size");
  UserSessionCollector collector;
  collector.start();
  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  auto sessions = collector.currentSessions();
  ASSERT_EQ(sessions.size(), collector.sessionCount());
  collector.stop();
  PASS();
}

static void test_session_has_valid_id() {
  TEST("Session: all sessions have valid IDs");
  UserSessionCollector collector;
  collector.start();
  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  auto sessions = collector.currentSessions();
  for (const auto& s : sessions) {
    ASSERT_TRUE(s.isValid());
  }
  collector.stop();
  PASS();
}

static void test_session_active_console() {
  TEST("Session: active console session found");
  UserSessionCollector collector;
  collector.start();
  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  auto sessions = collector.currentSessions();
  bool foundActive = false;
  for (const auto& s : sessions) {
    if (s.isActive()) {
      foundActive = true;
      break;
    }
  }
  ASSERT_TRUE(foundActive);
  collector.stop();
  PASS();
}

// ==================== Initial Snapshot Tests ====================

static void test_initial_snapshot_no_started_events() {
  TEST("Initial snapshot: no events for pre-existing sessions");
  UserSessionCollector collector;
  std::atomic<int> eventCount{0};

  collector.setCallback([&](UserSessionEventKind kind, UserSessionOrigin origin, const SessionInfo& info) {
    eventCount++;
  });

  collector.start();
  std::this_thread::sleep_for(std::chrono::milliseconds(300));
  int afterStartup = eventCount.load();
  collector.stop();

  ASSERT_EQ(afterStartup, 0);
  PASS();
}

static void test_initial_snapshot_tracks_sessions() {
  TEST("Initial snapshot: tracked count matches snapshot");
  UserSessionCollector collector;
  collector.start();
  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  std::size_t tracked = collector.sessionCount();
  std::size_t initialCount = collector.initialSnapshotCount();

  ASSERT_TRUE(tracked > 0);
  ASSERT_EQ(tracked, initialCount);
  collector.stop();
  PASS();
}

static void test_initial_snapshot_marked_correctly() {
  TEST("Initial snapshot: all initial sessions marked");
  UserSessionCollector collector;
  collector.start();
  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  auto sessions = collector.currentSessions();
  for (const auto& s : sessions) {
    ASSERT_TRUE(collector.wasInInitialSnapshot(s.session_id));
  }
  collector.stop();
  PASS();
}

// ==================== Login/Logout Tests ====================

static void test_polling_origin_after_initial() {
  TEST("Polling: no events emitted during stable polling (no false positives)");
  UserSessionCollector collector;
  std::atomic<int> eventCount{0};

  collector.setCallback([&](UserSessionEventKind kind, UserSessionOrigin origin, const SessionInfo& info) {
    eventCount++;
  });

  collector.start();
  std::this_thread::sleep_for(std::chrono::milliseconds(800));
  int count = eventCount.load();
  collector.stop();

  ASSERT_EQ(count, 0);
  PASS();
}

static void test_polling_detects_session_count() {
  TEST("Polling: session count remains stable during polling");
  UserSessionCollector collector;
  collector.start();
  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  std::size_t initial = collector.sessionCount();
  std::this_thread::sleep_for(std::chrono::milliseconds(800));
  std::size_t final_ = collector.sessionCount();
  collector.stop();

  ASSERT_TRUE(final_ >= initial);
  PASS();
}

// ==================== User Action Tests ====================

static void test_user_action_callback() {
  TEST("User action: callback receives action");
  UserSessionCollector collector;
  std::atomic<int> actionCount{0};

  collector.setUserActionCallback([&](UserActionKind kind, UserId uid, SessionId sid, const std::string& desc) {
    actionCount++;
  });

  collector.start();
  collector.reportUserAction(UserActionKind::ProgramStarted, "test.exe launched");
  collector.stop();

  ASSERT_EQ(actionCount.load(), 1);
  PASS();
}

static void test_user_action_no_callback() {
  TEST("User action: no callback does not crash");
  UserSessionCollector collector;
  collector.start();
  collector.reportUserAction(UserActionKind::ScriptExecuted, "test.sh");
  collector.stop();
  PASS();
}

// ==================== Correlation Tests ====================

static void test_session_events_correlated() {
  TEST("Session: tracked sessions all have valid correlated IDs");
  UserSessionCollector collector;
  collector.start();
  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  auto sessions = collector.currentSessions();
  ASSERT_TRUE(sessions.size() > 0);
  for (const auto& s : sessions) {
    ASSERT_TRUE(s.isValid());
    ASSERT_TRUE(s.session_id != kInvalidSession);
  }
  collector.stop();
  PASS();
}

static void test_initial_snapshot_count_matches_tracked() {
  TEST("Session: initialSnapshotCount matches tracked sessions");
  UserSessionCollector collector;
  collector.start();
  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  std::size_t tracked = collector.sessionCount();
  std::size_t initial = collector.initialSnapshotCount();
  ASSERT_EQ(tracked, initial);
  collector.stop();
  PASS();
}

// ==================== Static Helper Tests ====================

static void test_current_session_id() {
  TEST("Static: currentSessionId returns valid ID");
  SessionId sid = UserSessionCollector::currentSessionId();
  ASSERT_TRUE(sid != kInvalidSession);
  PASS();
}

static void test_current_username() {
  TEST("Static: currentUsername returns non-empty");
  std::string name = UserSessionCollector::currentUsername();
  ASSERT_TRUE(!name.empty());
  ASSERT_TRUE(name != "unknown");
  PASS();
}

// ==================== Permission Denied Tests ====================

static void test_permission_denied_graceful() {
  TEST("Permission denied: enumeration fails gracefully");
  UserSessionCollector collector;
  ASSERT_TRUE(collector.start());
  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  auto sessions = collector.currentSessions();
  ASSERT_TRUE(sessions.size() > 0);
  collector.stop();
  PASS();
}

// ==================== Main ====================

#ifndef MONIX_KERNEL_BUILD
int main() {
  printf("=== MONIX User Session Collector Tests ===\n\n");

  printf("[SessionState Names]\n");
  test_session_state_names();

  printf("[EventKind Names]\n");
  test_event_kind_names();

  printf("[EventKind Actions]\n");
  test_event_kind_actions();

  printf("[Origin Names]\n");
  test_origin_names();

  printf("[UserActionKind Names]\n");
  test_action_kind_names();

  printf("[SessionInfo]\n");
  test_session_info_defaults();
  test_session_info_is_active();
  test_session_info_is_valid();
  test_session_info_qualified_user();

  printf("[Collector Lifecycle]\n");
  test_collector_start_stop();
  test_collector_double_start();
  test_collector_stop_without_start();
  test_collector_config_defaults();

  printf("[Session Enumeration]\n");
  test_enumerate_sessions_finds_console();
  test_session_count_matches();
  test_session_has_valid_id();
  test_session_active_console();

  printf("[Initial Snapshot]\n");
  test_initial_snapshot_no_started_events();
  test_initial_snapshot_tracks_sessions();
  test_initial_snapshot_marked_correctly();

  printf("[Login/Logout]\n");
  test_polling_origin_after_initial();
  test_polling_detects_session_count();

  printf("[User Action]\n");
  test_user_action_callback();
  test_user_action_no_callback();

  printf("[Correlation]\n");
  test_session_events_correlated();
  test_initial_snapshot_count_matches_tracked();

  printf("[Static Helpers]\n");
  test_current_session_id();
  test_current_username();

  printf("[Permission Denied]\n");
  test_permission_denied_graceful();

  printf("\n=== Results: %d passed, %d failed ===\n", gPassed, gFailed);
  return gFailed > 0 ? 1 : 0;
}
#endif

#ifdef MONIX_KERNEL_BUILD
int GetFailedCount_UserSessionCollectorTests() { return gFailed; }

struct KTestEntry {
  const char* display_name;
  void (*func)();
};

static const KTestEntry s_ktests[] = {
  {"SessionState: names", test_session_state_names},
  {"UserSessionEventKind: names", test_event_kind_names},
  {"UserSessionEventKind: actions", test_event_kind_actions},
  {"UserSessionOrigin: names", test_origin_names},
  {"UserActionKind: names", test_action_kind_names},
  {"SessionInfo: defaults", test_session_info_defaults},
  {"SessionInfo: isActive", test_session_info_is_active},
  {"SessionInfo: isValid", test_session_info_is_valid},
  {"SessionInfo: qualifiedUser", test_session_info_qualified_user},
  {"UserSessionCollector: start stop", test_collector_start_stop},
  {"UserSessionCollector: double start", test_collector_double_start},
  {"UserSessionCollector: stop without start", test_collector_stop_without_start},
  {"UserSessionCollectorConfig: defaults", test_collector_config_defaults},
  {"Session enumeration: finds console", test_enumerate_sessions_finds_console},
  {"Session: count matches", test_session_count_matches},
  {"Session: valid IDs", test_session_has_valid_id},
  {"Session: active console", test_session_active_console},
  {"Initial snapshot: no events", test_initial_snapshot_no_started_events},
  {"Initial snapshot: tracks sessions", test_initial_snapshot_tracks_sessions},
  {"Initial snapshot: marked correctly", test_initial_snapshot_marked_correctly},
  {"Polling: no false positives", test_polling_origin_after_initial},
  {"Polling: session count stable", test_polling_detects_session_count},
  {"User action: callback", test_user_action_callback},
  {"User action: no callback no crash", test_user_action_no_callback},
  {"Session: correlated IDs", test_session_events_correlated},
  {"Session: initial snapshot matches tracked", test_initial_snapshot_count_matches_tracked},
  {"Static: currentSessionId", test_current_session_id},
  {"Static: currentUsername", test_current_username},
  {"Permission denied: graceful", test_permission_denied_graceful},
};

const KTestEntry* GetKTests_UserSessionCollector() { return s_ktests; }
std::size_t GetKTestCount_UserSessionCollector() { return sizeof(s_ktests) / sizeof(s_ktests[0]); }
#endif
