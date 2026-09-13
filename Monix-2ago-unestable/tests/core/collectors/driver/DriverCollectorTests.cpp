#include <cassert>
#include <iostream>
#include <string>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "DriverCollector.hpp"

using namespace monix::collectors::driver;

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

#define ASSERT_EQ(a, b) do { \
  if ((a) != (b)) { \
    std::cerr << "\n    FAIL: " << #a << " != " << #b << " (line " << __LINE__ << ")"; \
    return false; \
  } \
} while(0)

static bool test_event_kind_names() {
  ASSERT_EQ(std::string(DriverEventKindName(DriverEventKind::Loaded)), "Loaded");
  ASSERT_EQ(std::string(DriverEventKindName(DriverEventKind::Unloaded)), "Unloaded");
  ASSERT_EQ(std::string(DriverEventKindName(DriverEventKind::Changed)), "Changed");
  return true;
}

static bool test_event_kind_actions() {
  ASSERT_EQ(DriverEventKindAction(DriverEventKind::Loaded), "driver.loaded");
  ASSERT_EQ(DriverEventKindAction(DriverEventKind::Unloaded), "driver.unloaded");
  ASSERT_EQ(DriverEventKindAction(DriverEventKind::Changed), "driver.changed");
  return true;
}

static bool test_state_names() {
  ASSERT_EQ(std::string(DriverStateName(DriverState::Running)), "Running");
  ASSERT_EQ(std::string(DriverStateName(DriverState::Stopped)), "Stopped");
  ASSERT_EQ(std::string(DriverStateName(DriverState::StartPending)), "StartPending");
  ASSERT_EQ(std::string(DriverStateName(DriverState::StopPending)), "StopPending");
  ASSERT_EQ(std::string(DriverStateName(DriverState::Unknown)), "Unknown");
  return true;
}

static bool test_detection_origin_names() {
  ASSERT_EQ(std::string(DriverDetectionOriginName(DriverDetectionOrigin::Registry)), "Registry");
  ASSERT_EQ(std::string(DriverDetectionOriginName(DriverDetectionOrigin::EnumDeviceDrivers)), "EnumDeviceDrivers");
  ASSERT_EQ(std::string(DriverDetectionOriginName(DriverDetectionOrigin::Manual)), "Manual");
  ASSERT_EQ(std::string(DriverDetectionOriginName(DriverDetectionOrigin::Polling)), "Polling");
  return true;
}

static bool test_info_validity() {
  DriverInfo info;
  ASSERT_FALSE(info.isValid());
  info.name = "ntoskrnl.exe";
  ASSERT_TRUE(info.isValid());
  return true;
}

static bool test_info_is_running() {
  DriverInfo info;
  info.name = "test";
  info.state = DriverState::Running;
  ASSERT_TRUE(info.isRunning());
  info.state = DriverState::Stopped;
  ASSERT_FALSE(info.isRunning());
  return true;
}

static bool test_info_summary() {
  DriverInfo info;
  info.name = "ntoskrnl.exe";
  info.version = "10.0.26100.1";
  info.provider = "Microsoft";
  ASSERT_EQ(info.summary(), "ntoskrnl.exe v10.0.26100.1 by Microsoft");

  DriverInfo info2;
  info2.name = "ntoskrnl.exe";
  ASSERT_EQ(info2.summary(), "ntoskrnl.exe");
  return true;
}

static bool test_collector_lifecycle() {
  DriverCollectorConfig cfg;
  DriverCollector collector(cfg);
  ASSERT_FALSE(collector.isRunning());
  ASSERT_TRUE(collector.start());
  ASSERT_TRUE(collector.isRunning());
  ASSERT_FALSE(collector.start());
  ASSERT_TRUE(collector.stop());
  ASSERT_FALSE(collector.isRunning());
  ASSERT_FALSE(collector.stop());
  return true;
}

static bool test_collector_snapshot() {
  DriverCollector collector;
  ASSERT_TRUE(collector.start());
  auto snap = collector.snapshot();
  ASSERT_TRUE(snap.size() > 0);
  bool found_known_driver = false;
  for (const auto& d : snap) {
    if (d.name == "ACPI" || d.name == "disk" || d.name == "ndis" || d.name == "ntoskrnl" || d.name == "ntoskrnl.exe") {
      found_known_driver = true;
      ASSERT_TRUE(d.isValid());
    }
  }
  ASSERT_TRUE(found_known_driver);
  ASSERT_TRUE(collector.stop());
  return true;
}

static bool test_collector_drivers_tracked() {
  DriverCollector collector;
  ASSERT_TRUE(collector.start());
  std::size_t count = collector.driversTracked();
  ASSERT_TRUE(count > 0);
  ASSERT_TRUE(collector.stop());
  return true;
}

static bool test_collector_poll() {
  DriverCollector collector;
  ASSERT_TRUE(collector.start());
  ASSERT_TRUE(collector.poll());
  ASSERT_TRUE(collector.driversTracked() > 0);
  ASSERT_TRUE(collector.stop());
  return true;
}

static bool test_collector_events_emitted() {
  DriverCollector collector;
  ASSERT_TRUE(collector.start());
  std::size_t before = collector.eventsEmitted();
  collector.poll();
  std::size_t after = collector.eventsEmitted();
  ASSERT_TRUE(after >= before);
  ASSERT_TRUE(collector.stop());
  return true;
}

static bool test_collector_callback() {
  DriverCollector collector;
  int event_count = 0;
  std::string last_name;

  collector.setCallback([&](DriverEventKind, DriverDetectionOrigin, const DriverInfo& info) {
    event_count++;
    last_name = info.name;
  });

  ASSERT_TRUE(collector.start());
  ASSERT_TRUE(event_count > 0);
  ASSERT_FALSE(last_name.empty());
  ASSERT_TRUE(collector.stop());
  return true;
}

static bool test_collector_loaded_event() {
  DriverCollector collector;
  bool got_loaded = false;

  collector.setCallback([&](DriverEventKind kind, DriverDetectionOrigin, const DriverInfo&) {
    if (kind == DriverEventKind::Loaded) got_loaded = true;
  });

  ASSERT_TRUE(collector.start());
  ASSERT_TRUE(got_loaded);
  ASSERT_TRUE(collector.stop());
  return true;
}

static bool test_collector_permission_not_running() {
  DriverCollector collector;
  ASSERT_FALSE(collector.poll());
  ASSERT_EQ(collector.driversTracked(), static_cast<std::size_t>(0));
  return true;
}

static bool test_duplicate_suppression() {
  DriverCollector collector;
  int event_count = 0;

  collector.setCallback([&](DriverEventKind, DriverDetectionOrigin, const DriverInfo&) {
    event_count++;
  });

  ASSERT_TRUE(collector.start());
  int first_count = event_count;
  collector.poll();
  ASSERT_EQ(event_count, first_count);
  ASSERT_TRUE(collector.stop());
  return true;
}

static bool test_config_defaults() {
  DriverCollectorConfig cfg;
  ASSERT_TRUE(cfg.track_loaded);
  ASSERT_TRUE(cfg.track_unloaded);
  ASSERT_TRUE(cfg.track_changed);
  ASSERT_TRUE(cfg.include_version);
  ASSERT_TRUE(cfg.include_provider);
  ASSERT_TRUE(cfg.include_path);
  return true;
}

static bool test_config_set() {
  DriverCollectorConfig cfg;
  cfg.track_changed = false;
  cfg.include_version = false;
  DriverCollector collector(cfg);
  ASSERT_FALSE(collector.config().track_changed);
  ASSERT_FALSE(collector.config().include_version);
  return true;
}

static bool test_snapshot_deterministic() {
  DriverCollector collector;
  ASSERT_TRUE(collector.start());
  auto s1 = collector.snapshot();
  auto s2 = collector.snapshot();
  ASSERT_EQ(s1.size(), s2.size());
  for (std::size_t i = 0; i < s1.size(); i++) {
    ASSERT_EQ(s1[i].name, s2[i].name);
    ASSERT_EQ(s1[i].state, s2[i].state);
  }
  ASSERT_TRUE(collector.stop());
  return true;
}

static bool test_double_start() {
  DriverCollector collector;
  ASSERT_TRUE(collector.start());
  ASSERT_FALSE(collector.start());
  ASSERT_TRUE(collector.stop());
  ASSERT_FALSE(collector.stop());
  return true;
}

#ifndef MONIX_KERNEL_BUILD
int main() {
  std::cerr << "=== MONIX Driver Collector Tests ===\n\n";

  RUN_TEST(event_kind_names);
  RUN_TEST(event_kind_actions);
  RUN_TEST(state_names);
  RUN_TEST(detection_origin_names);
  RUN_TEST(info_validity);
  RUN_TEST(info_is_running);
  RUN_TEST(info_summary);
  RUN_TEST(collector_lifecycle);
  RUN_TEST(collector_snapshot);
  RUN_TEST(collector_drivers_tracked);
  RUN_TEST(collector_poll);
  RUN_TEST(collector_events_emitted);
  RUN_TEST(collector_callback);
  RUN_TEST(collector_loaded_event);
  RUN_TEST(collector_permission_not_running);
  RUN_TEST(duplicate_suppression);
  RUN_TEST(config_defaults);
  RUN_TEST(config_set);
  RUN_TEST(snapshot_deterministic);
  RUN_TEST(double_start);

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
  {"Event kind names", test_event_kind_names},
  {"Event kind actions", test_event_kind_actions},
  {"State names", test_state_names},
  {"Detection origin names", test_detection_origin_names},
  {"Info validity", test_info_validity},
  {"Info is running", test_info_is_running},
  {"Info summary", test_info_summary},
  {"Collector lifecycle", test_collector_lifecycle},
  {"Collector snapshot", test_collector_snapshot},
  {"Collector drivers tracked", test_collector_drivers_tracked},
  {"Collector poll", test_collector_poll},
  {"Collector events emitted", test_collector_events_emitted},
  {"Collector callback", test_collector_callback},
  {"Collector loaded event", test_collector_loaded_event},
  {"Collector permission not running", test_collector_permission_not_running},
  {"Duplicate suppression", test_duplicate_suppression},
  {"Config defaults", test_config_defaults},
  {"Config set", test_config_set},
  {"Snapshot deterministic", test_snapshot_deterministic},
  {"Double start", test_double_start},
};

const KBoolTestEntry* GetKBoolTests_DriverCollector() { return s_kbooltests; }
std::size_t GetKBoolTestCount_DriverCollector() { return sizeof(s_kbooltests) / sizeof(s_kbooltests[0]); }
#endif
