#include <cassert>
#include <iostream>
#include <string>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winsvc.h>

#include "ServiceCollector.hpp"

using namespace monix::collectors::service;

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
  ASSERT_EQ(std::string(ServiceEventKindName(ServiceEventKind::Created)), "Created");
  ASSERT_EQ(std::string(ServiceEventKindName(ServiceEventKind::Deleted)), "Deleted");
  ASSERT_EQ(std::string(ServiceEventKindName(ServiceEventKind::Started)), "Started");
  ASSERT_EQ(std::string(ServiceEventKindName(ServiceEventKind::Stopped)), "Stopped");
  ASSERT_EQ(std::string(ServiceEventKindName(ServiceEventKind::Changed)), "Changed");
  return true;
}

static bool test_event_kind_actions() {
  ASSERT_EQ(ServiceEventKindAction(ServiceEventKind::Created), "service.created");
  ASSERT_EQ(ServiceEventKindAction(ServiceEventKind::Deleted), "service.deleted");
  ASSERT_EQ(ServiceEventKindAction(ServiceEventKind::Started), "service.started");
  ASSERT_EQ(ServiceEventKindAction(ServiceEventKind::Stopped), "service.stopped");
  ASSERT_EQ(ServiceEventKindAction(ServiceEventKind::Changed), "service.changed");
  return true;
}

static bool test_state_names() {
  ASSERT_EQ(std::string(ServiceStateName(ServiceState::Stopped)), "Stopped");
  ASSERT_EQ(std::string(ServiceStateName(ServiceState::StartPending)), "StartPending");
  ASSERT_EQ(std::string(ServiceStateName(ServiceState::StopPending)), "StopPending");
  ASSERT_EQ(std::string(ServiceStateName(ServiceState::Running)), "Running");
  ASSERT_EQ(std::string(ServiceStateName(ServiceState::ContinuePending)), "ContinuePending");
  ASSERT_EQ(std::string(ServiceStateName(ServiceState::PausePending)), "PausePending");
  ASSERT_EQ(std::string(ServiceStateName(ServiceState::Paused)), "Paused");
  ASSERT_EQ(std::string(ServiceStateName(ServiceState::Unknown)), "Unknown");
  return true;
}

static bool test_state_from_dword() {
  ASSERT_EQ(ServiceStateFromDword(SERVICE_STOPPED), ServiceState::Stopped);
  ASSERT_EQ(ServiceStateFromDword(SERVICE_START_PENDING), ServiceState::StartPending);
  ASSERT_EQ(ServiceStateFromDword(SERVICE_STOP_PENDING), ServiceState::StopPending);
  ASSERT_EQ(ServiceStateFromDword(SERVICE_RUNNING), ServiceState::Running);
  ASSERT_EQ(ServiceStateFromDword(SERVICE_CONTINUE_PENDING), ServiceState::ContinuePending);
  ASSERT_EQ(ServiceStateFromDword(SERVICE_PAUSE_PENDING), ServiceState::PausePending);
  ASSERT_EQ(ServiceStateFromDword(SERVICE_PAUSED), ServiceState::Paused);
  ASSERT_EQ(ServiceStateFromDword(999), ServiceState::Unknown);
  return true;
}

static bool test_start_type_names() {
  ASSERT_EQ(std::string(ServiceStartTypeName(ServiceStartType::Boot)), "Boot");
  ASSERT_EQ(std::string(ServiceStartTypeName(ServiceStartType::System)), "System");
  ASSERT_EQ(std::string(ServiceStartTypeName(ServiceStartType::AutoStart)), "AutoStart");
  ASSERT_EQ(std::string(ServiceStartTypeName(ServiceStartType::Demand)), "Demand");
  ASSERT_EQ(std::string(ServiceStartTypeName(ServiceStartType::Disabled)), "Disabled");
  ASSERT_EQ(std::string(ServiceStartTypeName(ServiceStartType::Unknown)), "Unknown");
  return true;
}

static bool test_start_type_from_dword() {
  ASSERT_EQ(ServiceStartTypeFromDword(SERVICE_BOOT_START), ServiceStartType::Boot);
  ASSERT_EQ(ServiceStartTypeFromDword(SERVICE_SYSTEM_START), ServiceStartType::System);
  ASSERT_EQ(ServiceStartTypeFromDword(SERVICE_AUTO_START), ServiceStartType::AutoStart);
  ASSERT_EQ(ServiceStartTypeFromDword(SERVICE_DEMAND_START), ServiceStartType::Demand);
  ASSERT_EQ(ServiceStartTypeFromDword(SERVICE_DISABLED), ServiceStartType::Disabled);
  ASSERT_EQ(ServiceStartTypeFromDword(999), ServiceStartType::Unknown);
  return true;
}

static bool test_detection_origin_names() {
  ASSERT_EQ(std::string(ServiceDetectionOriginName(ServiceDetectionOrigin::SCM)), "SCM");
  ASSERT_EQ(std::string(ServiceDetectionOriginName(ServiceDetectionOrigin::Manual)), "Manual");
  ASSERT_EQ(std::string(ServiceDetectionOriginName(ServiceDetectionOrigin::Polling)), "Polling");
  return true;
}

static bool test_info_validity() {
  ServiceInfo info;
  ASSERT_FALSE(info.isValid());
  info.name = "test_svc";
  ASSERT_TRUE(info.isValid());
  return true;
}

static bool test_info_is_running() {
  ServiceInfo info;
  info.name = "test";
  info.state = ServiceState::Running;
  ASSERT_TRUE(info.isRunning());
  info.state = ServiceState::Stopped;
  ASSERT_FALSE(info.isRunning());
  return true;
}

static bool test_info_is_stopped() {
  ServiceInfo info;
  info.name = "test";
  info.state = ServiceState::Stopped;
  ASSERT_TRUE(info.isStopped());
  info.state = ServiceState::Running;
  ASSERT_FALSE(info.isStopped());
  return true;
}

static bool test_info_summary() {
  ServiceInfo info;
  info.name = "test_svc";
  info.display_name = "Test Service";
  ASSERT_EQ(info.summary(), "Test Service");
  info.display_name.clear();
  ASSERT_EQ(info.summary(), "test_svc");
  return true;
}

static bool test_collector_lifecycle() {
  ServiceCollectorConfig cfg;
  cfg.poll_interval_ms = std::chrono::milliseconds(100);
  ServiceCollector collector(cfg);

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
  ServiceCollector collector;
  ASSERT_TRUE(collector.start());
  auto snap = collector.snapshot();
  ASSERT_TRUE(snap.size() > 0);
  bool found_rpcss = false;
  for (const auto& svc : snap) {
    if (svc.name == "RpcSs" || svc.name == "RpcEptMapper") {
      found_rpcss = true;
      ASSERT_TRUE(svc.isValid());
      ASSERT_FALSE(svc.display_name.empty());
    }
  }
  ASSERT_TRUE(found_rpcss);
  ASSERT_TRUE(collector.stop());
  return true;
}

static bool test_collector_services_tracked() {
  ServiceCollector collector;
  ASSERT_TRUE(collector.start());
  std::size_t count = collector.servicesTracked();
  ASSERT_TRUE(count > 0);
  ASSERT_TRUE(collector.stop());
  return true;
}

static bool test_collector_poll() {
  ServiceCollector collector;
  ASSERT_TRUE(collector.start());
  ASSERT_TRUE(collector.poll());
  ASSERT_TRUE(collector.servicesTracked() > 0);
  ASSERT_TRUE(collector.stop());
  return true;
}

static bool test_collector_events_emitted() {
  ServiceCollector collector;
  ASSERT_TRUE(collector.start());
  std::size_t before = collector.eventsEmitted();
  collector.poll();
  std::size_t after = collector.eventsEmitted();
  ASSERT_TRUE(after >= before);
  ASSERT_TRUE(collector.stop());
  return true;
}

static bool test_collector_callback() {
  ServiceCollector collector;
  int event_count = 0;
  std::string last_name;

  collector.setCallback([&](ServiceEventKind, ServiceDetectionOrigin, const ServiceInfo& info) {
    event_count++;
    last_name = info.name;
  });

  ASSERT_TRUE(collector.start());
  ASSERT_TRUE(event_count > 0);
  ASSERT_FALSE(last_name.empty());
  ASSERT_TRUE(collector.stop());
  return true;
}

static bool test_collector_created_event() {
  ServiceCollector collector;
  bool got_created = false;

  collector.setCallback([&](ServiceEventKind kind, ServiceDetectionOrigin, const ServiceInfo&) {
    if (kind == ServiceEventKind::Created) got_created = true;
  });

  ASSERT_TRUE(collector.start());
  ASSERT_TRUE(got_created);
  ASSERT_TRUE(collector.stop());
  return true;
}

static bool test_collector_permission_not_running() {
  ServiceCollector collector;
  ASSERT_FALSE(collector.poll());
  ASSERT_EQ(collector.servicesTracked(), static_cast<std::size_t>(0));
  return true;
}

static bool test_config_defaults() {
  ServiceCollectorConfig cfg;
  ASSERT_TRUE(cfg.track_created);
  ASSERT_TRUE(cfg.track_deleted);
  ASSERT_TRUE(cfg.track_started);
  ASSERT_TRUE(cfg.track_stopped);
  ASSERT_TRUE(cfg.track_changed);
  ASSERT_FALSE(cfg.include_account);
  ASSERT_FALSE(cfg.include_binary_path);
  ASSERT_FALSE(cfg.include_description);
  return true;
}

static bool test_config_set() {
  ServiceCollectorConfig cfg;
  cfg.include_account = true;
  cfg.include_binary_path = true;
  ServiceCollector collector(cfg);
  ASSERT_TRUE(collector.config().include_account);
  ASSERT_TRUE(collector.config().include_binary_path);

  ServiceCollectorConfig cfg2;
  cfg2.include_account = false;
  collector.setConfig(cfg2);
  ASSERT_FALSE(collector.config().include_account);
  return true;
}

static bool test_snapshot_deterministic() {
  ServiceCollector collector;
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
  ServiceCollector collector;
  ASSERT_TRUE(collector.start());
  ASSERT_FALSE(collector.start());
  ASSERT_TRUE(collector.stop());
  ASSERT_FALSE(collector.stop());
  return true;
}

static bool test_collector_callback_no_duplicate() {
  ServiceCollector collector;
  int created_count = 0;

  collector.setCallback([&](ServiceEventKind kind, ServiceDetectionOrigin, const ServiceInfo&) {
    if (kind == ServiceEventKind::Created) created_count++;
  });

  ASSERT_TRUE(collector.start());
  int first_created = created_count;
  collector.poll();
  ASSERT_EQ(created_count, first_created);
  ASSERT_TRUE(collector.stop());
  return true;
}

#ifndef MONIX_KERNEL_BUILD
int main() {
  std::cerr << "=== MONIX Service Collector Tests ===\n\n";

  RUN_TEST(event_kind_names);
  RUN_TEST(event_kind_actions);
  RUN_TEST(state_names);
  RUN_TEST(state_from_dword);
  RUN_TEST(start_type_names);
  RUN_TEST(start_type_from_dword);
  RUN_TEST(detection_origin_names);
  RUN_TEST(info_validity);
  RUN_TEST(info_is_running);
  RUN_TEST(info_is_stopped);
  RUN_TEST(info_summary);
  RUN_TEST(collector_lifecycle);
  RUN_TEST(collector_snapshot);
  RUN_TEST(collector_services_tracked);
  RUN_TEST(collector_poll);
  RUN_TEST(collector_events_emitted);
  RUN_TEST(collector_callback);
  RUN_TEST(collector_created_event);
  RUN_TEST(collector_permission_not_running);
  RUN_TEST(config_defaults);
  RUN_TEST(config_set);
  RUN_TEST(snapshot_deterministic);
  RUN_TEST(double_start);
  RUN_TEST(collector_callback_no_duplicate);

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
  {"State from DWORD", test_state_from_dword},
  {"Start type names", test_start_type_names},
  {"Start type from DWORD", test_start_type_from_dword},
  {"Detection origin names", test_detection_origin_names},
  {"Info validity", test_info_validity},
  {"Info is running", test_info_is_running},
  {"Info is stopped", test_info_is_stopped},
  {"Info summary", test_info_summary},
  {"Collector lifecycle", test_collector_lifecycle},
  {"Collector snapshot", test_collector_snapshot},
  {"Collector services tracked", test_collector_services_tracked},
  {"Collector poll", test_collector_poll},
  {"Collector events emitted", test_collector_events_emitted},
  {"Collector callback", test_collector_callback},
  {"Collector created event", test_collector_created_event},
  {"Collector permission not running", test_collector_permission_not_running},
  {"Config defaults", test_config_defaults},
  {"Config set", test_config_set},
  {"Snapshot deterministic", test_snapshot_deterministic},
  {"Double start", test_double_start},
  {"Collector callback no duplicate", test_collector_callback_no_duplicate},
};

const KBoolTestEntry* GetKBoolTests_ServiceCollector() { return s_kbooltests; }
std::size_t GetKBoolTestCount_ServiceCollector() { return sizeof(s_kbooltests) / sizeof(s_kbooltests[0]); }
#endif
