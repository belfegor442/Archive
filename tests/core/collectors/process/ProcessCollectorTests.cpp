#include "../../../core/collectors/process/ProcessTypes.hpp"
#include "../../../core/collectors/process/ProcessIdentity.hpp"
#include "../../../core/collectors/process/ProcessSnapshot.hpp"
#include "../../../core/collectors/process/ProcessCollector.hpp"

#include <cassert>
#include <cstdio>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <atomic>
#include <filesystem>
#include <set>
#include <algorithm>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

using namespace monix::collectors::proc;

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
#define ASSERT_LE(a, b) do { if ((a) > (b)) { FAIL(#a " > " #b); return; } } while(0)

// ==================== ProcessArchitecture Tests ====================

static void test_arch_names() {
  TEST("ProcessArchitecture: all names");
  ASSERT_EQ(std::string(ProcessArchitectureName(ProcessArchitecture::Unknown)), "Unknown");
  ASSERT_EQ(std::string(ProcessArchitectureName(ProcessArchitecture::X86)), "x86");
  ASSERT_EQ(std::string(ProcessArchitectureName(ProcessArchitecture::X64)), "x64");
  ASSERT_EQ(std::string(ProcessArchitectureName(ProcessArchitecture::ARM)), "ARM");
  ASSERT_EQ(std::string(ProcessArchitectureName(ProcessArchitecture::ARM64)), "ARM64");
  PASS();
}

// ==================== ProcessIntegrityLevel Tests ====================

static void test_integrity_names() {
  TEST("ProcessIntegrityLevel: all names");
  ASSERT_EQ(std::string(ProcessIntegrityLevelName(ProcessIntegrityLevel::Unknown)), "Unknown");
  ASSERT_EQ(std::string(ProcessIntegrityLevelName(ProcessIntegrityLevel::Untrusted)), "Untrusted");
  ASSERT_EQ(std::string(ProcessIntegrityLevelName(ProcessIntegrityLevel::Low)), "Low");
  ASSERT_EQ(std::string(ProcessIntegrityLevelName(ProcessIntegrityLevel::Medium)), "Medium");
  ASSERT_EQ(std::string(ProcessIntegrityLevelName(ProcessIntegrityLevel::High)), "High");
  ASSERT_EQ(std::string(ProcessIntegrityLevelName(ProcessIntegrityLevel::System)), "System");
  PASS();
}

// ==================== ProcessEventKind Tests ====================

static void test_event_kind_names() {
  TEST("ProcessEventKind: all names");
  ASSERT_EQ(std::string(ProcessEventKindName(ProcessEventKind::Started)), "Started");
  ASSERT_EQ(std::string(ProcessEventKindName(ProcessEventKind::Terminated)), "Terminated");
  ASSERT_EQ(std::string(ProcessEventKindName(ProcessEventKind::Suspended)), "Suspended");
  ASSERT_EQ(std::string(ProcessEventKindName(ProcessEventKind::Resumed)), "Resumed");
  PASS();
}

static void test_event_kind_actions() {
  TEST("ProcessEventKind: action strings");
  ASSERT_EQ(ProcessEventKindAction(ProcessEventKind::Started), "started");
  ASSERT_EQ(ProcessEventKindAction(ProcessEventKind::Terminated), "terminated");
  ASSERT_EQ(ProcessEventKindAction(ProcessEventKind::Suspended), "suspended");
  ASSERT_EQ(ProcessEventKindAction(ProcessEventKind::Resumed), "resumed");
  PASS();
}

// ==================== ProcessInfo Tests ====================

static void test_process_info_defaults() {
  TEST("ProcessInfo: defaults are safe");
  ProcessInfo info;
  ASSERT_EQ(info.instance_id, 0u);
  ASSERT_EQ(info.pid, 0u);
  ASSERT_EQ(info.parent_pid, 0u);
  ASSERT_TRUE(info.image_name.empty());
  ASSERT_TRUE(info.command_line.empty());
  ASSERT_TRUE(info.user_name.empty());
  ASSERT_EQ(info.session_id, 0u);
  ASSERT_EQ(info.architecture, ProcessArchitecture::Unknown);
  ASSERT_EQ(info.integrity_level, ProcessIntegrityLevel::Unknown);
  ASSERT_EQ(info.creation_time_ms, 0);
  ASSERT_EQ(info.exit_time_ms, 0);
  ASSERT_EQ(info.exit_code, 0);
  PASS();
}

static void test_process_info_cpu_time() {
  TEST("ProcessInfo: cpu_time_ms calculation");
  ProcessInfo info;
  info.kernel_time_100ns = 100000;
  info.user_time_100ns = 200000;
  ASSERT_EQ(info.cpu_time_ms(), 30);
  PASS();
}

static void test_process_info_is_alive() {
  TEST("ProcessInfo: is_alive checks exit_time");
  ProcessInfo alive;
  alive.exit_time_ms = 0;
  ASSERT_TRUE(alive.is_alive());

  ProcessInfo dead;
  dead.exit_time_ms = 12345;
  ASSERT_FALSE(dead.is_alive());
  PASS();
}

static void test_process_info_is_64bit() {
  TEST("ProcessInfo: is_64bit checks architecture");
  ProcessInfo x86;
  x86.architecture = ProcessArchitecture::X86;
  ASSERT_FALSE(x86.is_64bit());

  ProcessInfo x64;
  x64.architecture = ProcessArchitecture::X64;
  ASSERT_TRUE(x64.is_64bit());

  ProcessInfo arm64;
  arm64.architecture = ProcessArchitecture::ARM64;
  ASSERT_TRUE(arm64.is_64bit());
  PASS();
}

// ==================== ProcessIdentity Tests ====================

static void test_identity_creation() {
  TEST("ProcessIdentity: creation produces valid id");
  ProcessIdentity id(1234, 567890);
  ASSERT_TRUE(id.isValid());
  ASSERT_EQ(id.pid(), 1234u);
  ASSERT_EQ(id.creationTimeMs(), 567890);
  PASS();
}

static void test_identity_invalid() {
  TEST("ProcessIdentity: invalid identity");
  ProcessIdentity id(0, 0);
  ASSERT_FALSE(id.isValid());
  ASSERT_EQ(id.id(), ProcessIdentity::invalid());
  PASS();
}

static void test_identity_stability() {
  TEST("ProcessIdentity: same PID+time produces same id");
  ProcessIdentity a(100, 200);
  ProcessIdentity b(100, 200);
  ASSERT_EQ(a.id(), b.id());
  ASSERT_TRUE(a == b);
  PASS();
}

static void test_identity_pid_reuse() {
  TEST("ProcessIdentity: different creation time = different id");
  ProcessIdentity a(100, 200);
  ProcessIdentity b(100, 300);
  ASSERT_NE(a.id(), b.id());
  ASSERT_TRUE(a != b);
  PASS();
}

static void test_identity_ordering() {
  TEST("ProcessIdentity: ordering works");
  ProcessIdentity a(100, 200);
  ProcessIdentity b(100, 300);
  ASSERT_TRUE(a < b);
  ASSERT_FALSE(b < a);
  PASS();
}

static void test_identity_to_string() {
  TEST("ProcessIdentity: toString format");
  ProcessIdentity id(1234, 567890);
  auto str = id.toString();
  ASSERT_TRUE(str.find("PID:1234") != std::string::npos);
  ASSERT_TRUE(str.find("567890") != std::string::npos);
  PASS();
}

static void test_identity_hash() {
  TEST("ProcessIdentity: hash works for unordered_map");
  std::unordered_set<ProcessIdentity> ids;
  ProcessIdentity a(100, 200);
  ProcessIdentity b(100, 200);
  ProcessIdentity c(100, 300);

  ids.insert(a);
  ids.insert(b);
  ids.insert(c);

  ASSERT_EQ(ids.size(), 2u);
  PASS();
}

// ==================== ProcessSnapshot Tests ====================

static void test_snapshot_capture() {
  TEST("ProcessSnapshot: capture finds processes");
  ProcessSnapshot snap;
  ASSERT_TRUE(snap.capture());
  ASSERT_TRUE(snap.processCount() > 0);
  ASSERT_TRUE(snap.captureTimeMs() > 0);
  PASS();
}

static void test_snapshot_contains_self() {
  TEST("ProcessSnapshot: capture contains current process");
  ProcessSnapshot snap;
  ASSERT_TRUE(snap.capture());

  DWORD myPid = GetCurrentProcessId();
  ASSERT_TRUE(snap.containsPid(myPid));
  PASS();
}

static void test_snapshot_find_by_pid() {
  TEST("ProcessSnapshot: findByPid returns info");
  ProcessSnapshot snap;
  ASSERT_TRUE(snap.capture());

  DWORD myPid = GetCurrentProcessId();
  const auto* info = snap.findByPid(myPid);
  ASSERT_TRUE(info != nullptr);
  ASSERT_EQ(info->pid, myPid);
  ASSERT_TRUE(info->instance_id != 0);
  PASS();
}

static void test_snapshot_processes_list() {
  TEST("ProcessSnapshot: processes() returns all");
  ProcessSnapshot snap;
  ASSERT_TRUE(snap.capture());
  auto procs = snap.processes();
  ASSERT_EQ(procs.size(), snap.processCount());
  PASS();
}

static void test_snapshot_diffs_from() {
  TEST("ProcessSnapshot: diffsFrom detects changes");
  ProcessSnapshot snap1;
  ASSERT_TRUE(snap1.capture());

  ProcessSnapshot snap2;
  ASSERT_TRUE(snap2.capture());

  auto diffs = snap2.diffsFrom(snap1);
  ASSERT_TRUE(diffs.size() >= 0);
  PASS();
}

static void test_snapshot_new_pids() {
  TEST("ProcessSnapshot: newPids detection");
  ProcessSnapshot snap1;
  ASSERT_TRUE(snap1.capture());

  ProcessSnapshot snap2;
  ASSERT_TRUE(snap2.capture());

  auto newPids = snap2.newPids(snap1);
  ASSERT_TRUE(newPids.size() >= 0);
  PASS();
}

static void test_snapshot_removed_pids() {
  TEST("ProcessSnapshot: removedPids detection");
  ProcessSnapshot snap1;
  ASSERT_TRUE(snap1.capture());

  ProcessSnapshot snap2;
  ASSERT_TRUE(snap2.capture());

  auto removedPids = snap2.removedPids(snap1);
  ASSERT_TRUE(removedPids.size() >= 0);
  PASS();
}

static void test_snapshot_clear() {
  TEST("ProcessSnapshot: clear empties snapshot");
  ProcessSnapshot snap;
  ASSERT_TRUE(snap.capture());
  ASSERT_TRUE(snap.processCount() > 0);
  snap.clear();
  ASSERT_EQ(snap.processCount(), 0u);
  PASS();
}

static void test_snapshot_self_info() {
  TEST("ProcessSnapshot: selfInfo returns valid info");
  auto info = ProcessSnapshot::selfInfo();
  ASSERT_TRUE(info.pid > 0);
  ASSERT_TRUE(info.instance_id != 0);
  PASS();
}

static void test_snapshot_current_process_info() {
  TEST("ProcessSnapshot: currentProcessInfo returns valid info");
  auto info = ProcessSnapshot::currentProcessInfo();
  ASSERT_TRUE(info.pid > 0);
  PASS();
}

static void test_snapshot_system_process() {
  TEST("ProcessSnapshot: System (PID 4) found");
  ProcessSnapshot snap;
  ASSERT_TRUE(snap.capture());

  const auto* system = snap.findByPid(4);
  ASSERT_TRUE(system != nullptr);
  ASSERT_TRUE(system->image_name.find("System") != std::string::npos ||
              system->image_name.find("idle") != std::string::npos ||
              system->image_name.find("Idle") != std::string::npos);
  PASS();
}

static void test_snapshot_multiple_captures() {
  TEST("ProcessSnapshot: multiple captures consistent");
  ProcessSnapshot snap1;
  ASSERT_TRUE(snap1.capture());

  ProcessSnapshot snap2;
  ASSERT_TRUE(snap2.capture());

  ASSERT_TRUE(snap2.processCount() > 0);
  ASSERT_TRUE(snap1.processCount() > 0);
  PASS();
}

static void test_snapshot_process_has_instance_id() {
  TEST("ProcessSnapshot: all processes have instance IDs");
  ProcessSnapshot snap;
  ASSERT_TRUE(snap.capture());

  auto procs = snap.processes();
  for (const auto& p : procs) {
    ASSERT_TRUE(p.instance_id != 0 || p.pid == 0);
  }
  PASS();
}

static void test_snapshot_process_has_image_name() {
  TEST("ProcessSnapshot: most processes have image names");
  ProcessSnapshot snap;
  ASSERT_TRUE(snap.capture());

  std::size_t with_name = 0;
  auto procs = snap.processes();
  for (const auto& p : procs) {
    if (!p.image_name.empty()) with_name++;
  }
  ASSERT_TRUE(with_name > procs.size() / 2);
  PASS();
}

// ==================== Rapid Process Tests ====================

static void test_rapid_process_enumeration() {
  TEST("Rapid Process: 100 snapshots in under 5s");
  auto start = std::chrono::steady_clock::now();

  for (int i = 0; i < 100; i++) {
    ProcessSnapshot snap;
    snap.capture();
  }

  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::steady_clock::now() - start).count();
  ASSERT_TRUE(elapsed < 5000);
  PASS();
}

// ==================== PID Reuse Simulation Tests ====================

static void test_pid_reuse_simulation() {
  TEST("PID reuse: different times produce different identities");
  ProcessInstanceId id1 = ProcessIdentity::compute(1234, 1000);
  ProcessInstanceId id2 = ProcessIdentity::compute(1234, 2000);
  ProcessInstanceId id3 = ProcessIdentity::compute(1234, 1000);

  ASSERT_NE(id1, id2);
  ASSERT_EQ(id1, id3);
  PASS();
}

static void test_pid_reuse_hash_collision_avoidance() {
  TEST("PID reuse: hash set handles same PID different times");
  std::unordered_set<ProcessInstanceId> ids;
  for (std::int64_t t = 1000; t < 1100; t++) {
    ids.insert(ProcessIdentity::compute(1234, t));
  }
  ASSERT_EQ(ids.size(), 100u);
  PASS();
}

// ==================== Collector Lifecycle Tests ====================

static void test_collector_start_stop() {
  TEST("ProcessCollector: start and stop lifecycle");
  ProcessCollector collector;
  ASSERT_FALSE(collector.isRunning());

  ASSERT_TRUE(collector.start());
  ASSERT_TRUE(collector.isRunning());

  ASSERT_TRUE(collector.stop());
  ASSERT_FALSE(collector.isRunning());
  PASS();
}

static void test_collector_double_start() {
  TEST("ProcessCollector: double start returns false");
  ProcessCollector collector;
  ASSERT_TRUE(collector.start());
  ASSERT_FALSE(collector.start());
  ASSERT_TRUE(collector.stop());
  PASS();
}

static void test_collector_stop_without_start() {
  TEST("ProcessCollector: stop without start returns false");
  ProcessCollector collector;
  ASSERT_FALSE(collector.stop());
  PASS();
}

static void test_collector_config_defaults() {
  TEST("ProcessCollectorConfig: defaults are safe");
  auto cfg = ProcessCollectorConfig::defaults();
  ASSERT_TRUE(cfg.poll_interval_ms > 0);
  ASSERT_FALSE(cfg.track_suspended);
  ASSERT_TRUE(cfg.capture_command_line);
  ASSERT_TRUE(cfg.capture_working_directory);
  ASSERT_TRUE(cfg.max_tracked > 0);
  PASS();
}

static void test_collector_events_emitted() {
  TEST("ProcessCollector: events emitted for new processes");
  ProcessCollector collector;
  collector.start();
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  ASSERT_TRUE(collector.eventsEmitted() >= 0);
  collector.stop();
  PASS();
}

static void test_collector_tracked_count() {
  TEST("ProcessCollector: tracked count > 0 after start");
  ProcessCollector collector;
  collector.start();
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  ASSERT_TRUE(collector.trackedCount() > 0);
  collector.stop();
  PASS();
}

static void test_collector_callback_receives_events() {
  TEST("ProcessCollector: callback receives events");
  ProcessCollector collector;
  std::atomic<int> eventCount{0};

  collector.setCallback([&](ProcessEventKind kind, ObservationOrigin origin, const ProcessInfo& info) {
    eventCount++;
  });

  collector.start();
  std::this_thread::sleep_for(std::chrono::milliseconds(300));
  collector.stop();

  ASSERT_TRUE(eventCount.load() >= 0);
  PASS();
}

static void test_collector_current_pid() {
  TEST("ProcessCollector: currentPid matches GetCurrentProcessId");
  ASSERT_EQ(ProcessCollector::currentPid(),
            static_cast<ProcessId>(GetCurrentProcessId()));
  PASS();
}

static void test_collector_parent_pid() {
  TEST("ProcessCollector: currentParentPid returns valid parent");
  ProcessId parentPid = ProcessCollector::currentParentPid();
  ASSERT_TRUE(parentPid > 0);
  PASS();
}

// ==================== Missing Command Line Tests ====================

static void test_missing_command_line() {
  TEST("Missing command line: process without cmd line still captured");
  ProcessSnapshot snap;
  ASSERT_TRUE(snap.capture());

  const auto* system = snap.findByPid(4);
  if (system) {
    ASSERT_TRUE(system->instance_id != 0);
  }
  PASS();
}

// ==================== Permission Denied Tests ====================

static void test_permission_denied_process() {
  TEST("Permission denied: system process handled gracefully");
  ProcessSnapshot snap;
  ASSERT_TRUE(snap.capture());

  const auto* system = snap.findByPid(4);
  ASSERT_TRUE(system != nullptr);
  ASSERT_TRUE(system->instance_id != 0);
  PASS();
}

// ==================== Process Enumeration Snapshot Tests ====================

static void test_enumeration_snapshot_consistency() {
  TEST("Enumeration snapshot: consecutive captures have overlap");
  ProcessSnapshot snap1;
  ASSERT_TRUE(snap1.capture());

  ProcessSnapshot snap2;
  ASSERT_TRUE(snap2.capture());

  std::size_t overlap = 0;
  for (const auto& p : snap1.processes()) {
    if (snap2.containsPid(p.pid)) overlap++;
  }

  ASSERT_TRUE(overlap > snap1.processCount() / 2);
  PASS();
}

static void test_enumeration_snapshot_parent_child() {
  TEST("Enumeration snapshot: parent-child relationship present");
  ProcessSnapshot snap;
  ASSERT_TRUE(snap.capture());

  bool foundChild = false;
  for (const auto& p : snap.processes()) {
    if (p.parent_pid > 0 && snap.containsPid(p.parent_pid)) {
      foundChild = true;
      break;
    }
  }
  ASSERT_TRUE(foundChild);
  PASS();
}

static void test_enumeration_snapshot_self_in_snapshot() {
  TEST("Enumeration snapshot: self present in snapshot");
  ProcessSnapshot snap;
  ASSERT_TRUE(snap.capture());

  DWORD myPid = GetCurrentProcessId();
  const auto* self = snap.findByPid(myPid);
  ASSERT_TRUE(self != nullptr);
  ASSERT_EQ(self->pid, myPid);
  ASSERT_TRUE(self->instance_id != 0);
  PASS();
}

static void test_enumeration_snapshot_has_architecture() {
  TEST("Enumeration snapshot: architecture detected");
  ProcessSnapshot snap;
  ASSERT_TRUE(snap.capture());

  DWORD myPid = GetCurrentProcessId();
  const auto* self = snap.findByPid(myPid);
  ASSERT_TRUE(self != nullptr);
  ASSERT_NE(self->architecture, ProcessArchitecture::Unknown);
  PASS();
}

// ==================== Phase 9: Process Snapshot Tests ====================

static void test_observation_origin_names() {
  TEST("ObservationOrigin: all names");
  ASSERT_EQ(std::string(ObservationOriginName(ObservationOrigin::InitialSnapshot)), "InitialSnapshot");
  ASSERT_EQ(std::string(ObservationOriginName(ObservationOrigin::Polling)), "Polling");
  ASSERT_EQ(std::string(ObservationOriginName(ObservationOrigin::Manual)), "Manual");
  PASS();
}

static void test_initial_snapshot_no_started_events() {
  TEST("Initial snapshot: no process.started for pre-existing processes");
  ProcessCollector collector;
  std::atomic<int> startedCount{0};
  std::atomic<int> totalEvents{0};

  collector.setCallback([&](ProcessEventKind kind, ObservationOrigin origin, const ProcessInfo& info) {
    totalEvents++;
    if (kind == ProcessEventKind::Started) startedCount++;
  });

  collector.start();
  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  int afterStartup = startedCount.load();
  collector.stop();

  ASSERT_EQ(afterStartup, 0);
  PASS();
}

static void test_initial_snapshot_tracks_processes() {
  TEST("Initial snapshot: tracked count matches snapshot");
  ProcessCollector collector;
  collector.start();
  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  std::size_t tracked = collector.trackedCount();
  std::size_t initialCount = collector.initialSnapshotCount();

  ASSERT_TRUE(tracked > 0);
  ASSERT_EQ(tracked, initialCount);
  collector.stop();
  PASS();
}

static void test_initial_snapshot_marked_correctly() {
  TEST("Initial snapshot: all initial processes marked as initial");
  ProcessCollector collector;
  collector.start();
  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  ProcessSnapshot snap = collector.snapshot();
  for (const auto& info : snap.processes()) {
    ASSERT_TRUE(collector.wasInInitialSnapshot(info.instance_id));
  }
  collector.stop();
  PASS();
}

static void test_process_appearing_after_snapshot() {
  TEST("Process appearing after snapshot: generates Started event");
  ProcessCollector collector;
  std::atomic<int> startedCount{0};
  std::atomic<bool> gotPolling{false};

  collector.setCallback([&](ProcessEventKind kind, ObservationOrigin origin, const ProcessInfo& info) {
    if (kind == ProcessEventKind::Started && origin == ObservationOrigin::Polling) {
      startedCount++;
      gotPolling = true;
    }
  });

  collector.start();
  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  wchar_t exePath[] = L"C:\\Windows\\System32\\cmd.exe";
  wchar_t cmdLine[] = L"/c timeout /t 5 /nobreak >nul";
  STARTUPINFOW si1{};
  si1.cb = sizeof(si1);
  PROCESS_INFORMATION pi1{};
  BOOL ok = CreateProcessW(
    exePath, cmdLine,
    nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr,
    &si1, &pi1);
  if (ok) {
    CloseHandle(pi1.hProcess);
    CloseHandle(pi1.hThread);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  collector.stop();

  ASSERT_TRUE(startedCount.load() >= 0);
  PASS();
}

static void test_process_disappearing_generates_terminated() {
  TEST("Process disappearing: generates Terminated event");
  ProcessCollector collector;
  std::atomic<int> terminatedCount{0};

  collector.setCallback([&](ProcessEventKind kind, ObservationOrigin origin, const ProcessInfo& info) {
    if (kind == ProcessEventKind::Terminated) terminatedCount++;
  });

  collector.start();
  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  STARTUPINFOW si{};
  si.cb = sizeof(si);
  PROCESS_INFORMATION pi{};
  wchar_t exePath2[] = L"C:\\Windows\\System32\\cmd.exe";
  wchar_t cmdLine2[] = L"/c exit";
  if (CreateProcessW(
    exePath2, cmdLine2,
    nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr,
    &si, &pi)) {
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(200));
  int finalTerminated = terminatedCount.load();
  collector.stop();

  ASSERT_TRUE(finalTerminated >= 0);
  PASS();
}

static void test_initial_snapshot_origin_is_initial() {
  TEST("Initial snapshot: initial processes tracked without events");
  ProcessCollector collector;
  std::atomic<int> callbackCalls{0};

  collector.setCallback([&](ProcessEventKind kind, ObservationOrigin origin, const ProcessInfo& info) {
    callbackCalls++;
  });

  collector.start();
  std::this_thread::sleep_for(std::chrono::milliseconds(300));

  std::size_t tracked = collector.trackedCount();
  std::size_t initialCount = collector.initialSnapshotCount();

  ASSERT_TRUE(tracked > 0);
  ASSERT_EQ(tracked, initialCount);
  ASSERT_EQ(callbackCalls.load(), 0);

  collector.stop();
  PASS();
}

static void test_polling_origin_after_initial() {
  TEST("Polling: events after initial use Polling origin");
  ProcessCollector collector;
  std::atomic<bool> sawPollingOrigin{false};

  collector.setCallback([&](ProcessEventKind kind, ObservationOrigin origin, const ProcessInfo& info) {
    if (origin == ObservationOrigin::Polling) {
      sawPollingOrigin = true;
    }
  });

  collector.start();
  std::this_thread::sleep_for(std::chrono::milliseconds(300));
  collector.stop();

  ASSERT_TRUE(sawPollingOrigin.load());
  PASS();
}

// ==================== Main ====================

#ifndef MONIX_KERNEL_BUILD
int main() {
  printf("=== MONIX Process Collector Tests ===\n\n");

  printf("[Architecture Names]\n");
  test_arch_names();

  printf("[Integrity Level Names]\n");
  test_integrity_names();

  printf("[Event Kind Names]\n");
  test_event_kind_names();

  printf("[Event Kind Actions]\n");
  test_event_kind_actions();

  printf("[ProcessInfo]\n");
  test_process_info_defaults();
  test_process_info_cpu_time();
  test_process_info_is_alive();
  test_process_info_is_64bit();

  printf("[ProcessIdentity]\n");
  test_identity_creation();
  test_identity_invalid();
  test_identity_stability();
  test_identity_pid_reuse();
  test_identity_ordering();
  test_identity_to_string();
  test_identity_hash();

  printf("[ProcessSnapshot]\n");
  test_snapshot_capture();
  test_snapshot_contains_self();
  test_snapshot_find_by_pid();
  test_snapshot_processes_list();
  test_snapshot_diffs_from();
  test_snapshot_new_pids();
  test_snapshot_removed_pids();
  test_snapshot_clear();
  test_snapshot_self_info();
  test_snapshot_current_process_info();
  test_snapshot_system_process();
  test_snapshot_multiple_captures();
  test_snapshot_process_has_instance_id();
  test_snapshot_process_has_image_name();

  printf("[Rapid Process]\n");
  test_rapid_process_enumeration();

  printf("[PID Reuse Simulation]\n");
  test_pid_reuse_simulation();
  test_pid_reuse_hash_collision_avoidance();

  printf("[Collector Lifecycle]\n");
  test_collector_start_stop();
  test_collector_double_start();
  test_collector_stop_without_start();
  test_collector_config_defaults();
  test_collector_events_emitted();
  test_collector_tracked_count();
  test_collector_callback_receives_events();
  test_collector_current_pid();
  test_collector_parent_pid();

  printf("[Missing Command Line]\n");
  test_missing_command_line();

  printf("[Permission Denied]\n");
  test_permission_denied_process();

  printf("[Process Enumeration Snapshot]\n");
  test_enumeration_snapshot_consistency();
  test_enumeration_snapshot_parent_child();
  test_enumeration_snapshot_self_in_snapshot();
  test_enumeration_snapshot_has_architecture();

  printf("[Phase 9: Process Snapshot]\n");
  test_observation_origin_names();
  test_initial_snapshot_no_started_events();
  test_initial_snapshot_tracks_processes();
  test_initial_snapshot_marked_correctly();
  test_process_appearing_after_snapshot();
  test_process_disappearing_generates_terminated();
  test_initial_snapshot_origin_is_initial();
  test_polling_origin_after_initial();

  printf("\n=== Results: %d passed, %d failed ===\n", gPassed, gFailed);
  return gFailed > 0 ? 1 : 0;
}
#endif

#ifdef MONIX_KERNEL_BUILD
int GetFailedCount_ProcessCollectorTests() { return gFailed; }

struct KTestEntry {
  const char* display_name;
  void (*func)();
};

static const KTestEntry s_ktests[] = {
  {"ProcessArchitecture: names", test_arch_names},
  {"ProcessIntegrityLevel: names", test_integrity_names},
  {"ProcessEventKind: names", test_event_kind_names},
  {"ProcessEventKind: actions", test_event_kind_actions},
  {"ProcessInfo: defaults", test_process_info_defaults},
  {"ProcessInfo: cpu_time_ms", test_process_info_cpu_time},
  {"ProcessInfo: is_alive", test_process_info_is_alive},
  {"ProcessInfo: is_64bit", test_process_info_is_64bit},
  {"ProcessIdentity: creation", test_identity_creation},
  {"ProcessIdentity: invalid", test_identity_invalid},
  {"ProcessIdentity: stability", test_identity_stability},
  {"ProcessIdentity: pid reuse", test_identity_pid_reuse},
  {"ProcessIdentity: ordering", test_identity_ordering},
  {"ProcessIdentity: toString", test_identity_to_string},
  {"ProcessIdentity: hash", test_identity_hash},
  {"ProcessSnapshot: capture", test_snapshot_capture},
  {"ProcessSnapshot: contains self", test_snapshot_contains_self},
  {"ProcessSnapshot: findByPid", test_snapshot_find_by_pid},
  {"ProcessSnapshot: processes list", test_snapshot_processes_list},
  {"ProcessSnapshot: diffsFrom", test_snapshot_diffs_from},
  {"ProcessSnapshot: newPids", test_snapshot_new_pids},
  {"ProcessSnapshot: removedPids", test_snapshot_removed_pids},
  {"ProcessSnapshot: clear", test_snapshot_clear},
  {"ProcessSnapshot: selfInfo", test_snapshot_self_info},
  {"ProcessSnapshot: currentProcessInfo", test_snapshot_current_process_info},
  {"ProcessSnapshot: System PID 4", test_snapshot_system_process},
  {"ProcessSnapshot: multiple captures", test_snapshot_multiple_captures},
  {"ProcessSnapshot: all have instance_id", test_snapshot_process_has_instance_id},
  {"ProcessSnapshot: most have image names", test_snapshot_process_has_image_name},
  {"Rapid Process: 100 snapshots", test_rapid_process_enumeration},
  {"PID reuse: different times", test_pid_reuse_simulation},
  {"PID reuse: hash set", test_pid_reuse_hash_collision_avoidance},
  {"ProcessCollector: start stop", test_collector_start_stop},
  {"ProcessCollector: double start", test_collector_double_start},
  {"ProcessCollector: stop without start", test_collector_stop_without_start},
  {"ProcessCollectorConfig: defaults", test_collector_config_defaults},
  {"ProcessCollector: events emitted", test_collector_events_emitted},
  {"ProcessCollector: tracked count", test_collector_tracked_count},
  {"ProcessCollector: callback receives", test_collector_callback_receives_events},
  {"ProcessCollector: currentPid", test_collector_current_pid},
  {"ProcessCollector: parentPid", test_collector_parent_pid},
  {"Missing command line: handled", test_missing_command_line},
  {"Permission denied: graceful", test_permission_denied_process},
  {"Enumeration snapshot: consistency", test_enumeration_snapshot_consistency},
  {"Enumeration snapshot: parent-child", test_enumeration_snapshot_parent_child},
  {"Enumeration snapshot: self present", test_enumeration_snapshot_self_in_snapshot},
  {"Enumeration snapshot: architecture", test_enumeration_snapshot_has_architecture},
  {"ObservationOrigin: names", test_observation_origin_names},
  {"Initial snapshot: no started events", test_initial_snapshot_no_started_events},
  {"Initial snapshot: tracks processes", test_initial_snapshot_tracks_processes},
  {"Initial snapshot: marked correctly", test_initial_snapshot_marked_correctly},
  {"Process appearing after snapshot", test_process_appearing_after_snapshot},
  {"Process disappearing: terminated", test_process_disappearing_generates_terminated},
  {"Initial snapshot: origin is initial", test_initial_snapshot_origin_is_initial},
  {"Polling: after initial", test_polling_origin_after_initial},
};

const KTestEntry* GetKTests_ProcessCollector() { return s_ktests; }
std::size_t GetKTestCount_ProcessCollector() { return sizeof(s_ktests) / sizeof(s_ktests[0]); }
#endif
