#include <cassert>
#include <iostream>
#include <string>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "ConfigCollector.hpp"

using namespace monix::collectors::config;

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

static bool test_severity_names() {
  ASSERT_EQ(std::string(ConfigChangeSeverityName(ConfigChangeSeverity::Critical)), "Critical");
  ASSERT_EQ(std::string(ConfigChangeSeverityName(ConfigChangeSeverity::Important)), "Important");
  ASSERT_EQ(std::string(ConfigChangeSeverityName(ConfigChangeSeverity::Normal)), "Normal");
  ASSERT_EQ(std::string(ConfigChangeSeverityName(ConfigChangeSeverity::Ignored)), "Ignored");
  return true;
}

static bool test_severity_actions() {
  ASSERT_EQ(ConfigChangeSeverityAction(ConfigChangeSeverity::Critical), "config.critical");
  ASSERT_EQ(ConfigChangeSeverityAction(ConfigChangeSeverity::Important), "config.important");
  ASSERT_EQ(ConfigChangeSeverityAction(ConfigChangeSeverity::Normal), "config.normal");
  ASSERT_EQ(ConfigChangeSeverityAction(ConfigChangeSeverity::Ignored), "config.ignored");
  return true;
}

static bool test_area_names() {
  ASSERT_EQ(std::string(ConfigAreaName(ConfigArea::Security)), "Security");
  ASSERT_EQ(std::string(ConfigAreaName(ConfigArea::Firewall)), "Firewall");
  ASSERT_EQ(std::string(ConfigAreaName(ConfigArea::Startup)), "Startup");
  ASSERT_EQ(std::string(ConfigAreaName(ConfigArea::Services)), "Services");
  ASSERT_EQ(std::string(ConfigAreaName(ConfigArea::Drivers)), "Drivers");
  ASSERT_EQ(std::string(ConfigAreaName(ConfigArea::Policies)), "Policies");
  ASSERT_EQ(std::string(ConfigAreaName(ConfigArea::SystemConfiguration)), "SystemConfiguration");
  return true;
}

static bool test_area_from_name() {
  ASSERT_EQ(ConfigAreaFromName("Security"), ConfigArea::Security);
  ASSERT_EQ(ConfigAreaFromName("Firewall"), ConfigArea::Firewall);
  ASSERT_EQ(ConfigAreaFromName("Startup"), ConfigArea::Startup);
  ASSERT_EQ(ConfigAreaFromName("Unknown"), ConfigArea::SystemConfiguration);
  return true;
}

static bool test_change_kind_names() {
  ASSERT_EQ(std::string(ConfigChangeKindName(ConfigChangeKind::Added)), "Added");
  ASSERT_EQ(std::string(ConfigChangeKindName(ConfigChangeKind::Removed)), "Removed");
  ASSERT_EQ(std::string(ConfigChangeKindName(ConfigChangeKind::Modified)), "Modified");
  return true;
}

static bool test_detection_origin_names() {
  ASSERT_EQ(std::string(ConfigDetectionOriginName(ConfigDetectionOrigin::Registry)), "Registry");
  ASSERT_EQ(std::string(ConfigDetectionOriginName(ConfigDetectionOrigin::Polling)), "Polling");
  ASSERT_EQ(std::string(ConfigDetectionOriginName(ConfigDetectionOrigin::Manual)), "Manual");
  return true;
}

static bool test_scope_area_enabled() {
  ConfigScope scope;
  ASSERT_TRUE(scope.isAreaEnabled(ConfigArea::Security));
  ASSERT_TRUE(scope.isAreaEnabled(ConfigArea::Firewall));
  scope.areas = {ConfigArea::Security, ConfigArea::Firewall};
  ASSERT_TRUE(scope.isAreaEnabled(ConfigArea::Security));
  ASSERT_TRUE(scope.isAreaEnabled(ConfigArea::Firewall));
  ASSERT_FALSE(scope.isAreaEnabled(ConfigArea::Startup));
  return true;
}

static bool test_scope_severity_visible() {
  ConfigScope scope;
  scope.min_severity = ConfigChangeSeverity::Important;
  ASSERT_TRUE(scope.isSeverityVisible(ConfigChangeSeverity::Critical));
  ASSERT_TRUE(scope.isSeverityVisible(ConfigChangeSeverity::Important));
  ASSERT_FALSE(scope.isSeverityVisible(ConfigChangeSeverity::Normal));
  ASSERT_FALSE(scope.isSeverityVisible(ConfigChangeSeverity::Ignored));

  scope.include_ignored = true;
  ASSERT_TRUE(scope.isSeverityVisible(ConfigChangeSeverity::Ignored));
  return true;
}

static bool test_scope_matches() {
  ConfigScope scope;
  scope.areas = {ConfigArea::Security};
  scope.min_severity = ConfigChangeSeverity::Important;

  ConfigChange change;
  change.area = ConfigArea::Security;
  change.severity = ConfigChangeSeverity::Critical;
  ASSERT_TRUE(scope.matches(change));

  change.area = ConfigArea::Startup;
  ASSERT_FALSE(scope.matches(change));

  change.area = ConfigArea::Security;
  change.severity = ConfigChangeSeverity::Normal;
  ASSERT_FALSE(scope.matches(change));
  return true;
}

static bool test_change_validity() {
  ConfigChange change;
  ASSERT_FALSE(change.isValid());
  change.key_path = "test\\path";
  ASSERT_TRUE(change.isValid());
  return true;
}

static bool test_change_severity_checks() {
  ConfigChange change;
  change.severity = ConfigChangeSeverity::Critical;
  ASSERT_TRUE(change.isCritical());
  ASSERT_FALSE(change.isImportant());
  ASSERT_FALSE(change.isIgnored());

  change.severity = ConfigChangeSeverity::Important;
  ASSERT_FALSE(change.isCritical());
  ASSERT_TRUE(change.isImportant());
  ASSERT_FALSE(change.isIgnored());

  change.severity = ConfigChangeSeverity::Ignored;
  ASSERT_FALSE(change.isCritical());
  ASSERT_FALSE(change.isImportant());
  ASSERT_TRUE(change.isIgnored());
  return true;
}

static bool test_change_summary() {
  ConfigChange change;
  change.severity = ConfigChangeSeverity::Critical;
  change.area = ConfigArea::Security;
  change.kind = ConfigChangeKind::Modified;
  change.key_path = "Lsa\\Test";
  change.value_name = "Value";
  std::string s = change.summary();
  ASSERT_TRUE(s.find("Critical") != std::string::npos);
  ASSERT_TRUE(s.find("Security") != std::string::npos);
  ASSERT_TRUE(s.find("Modified") != std::string::npos);
  return true;
}

static bool test_collector_lifecycle() {
  ConfigCollectorConfig cfg;
  ConfigCollector collector(cfg);
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
  ConfigCollector collector;
  ASSERT_TRUE(collector.start());
  auto snap = collector.snapshot();
  ASSERT_TRUE(snap.size() > 0);
  ASSERT_TRUE(collector.stop());
  return true;
}

static bool test_collector_changes_tracked() {
  ConfigCollector collector;
  ASSERT_TRUE(collector.start());
  ASSERT_TRUE(collector.changesTracked() > 0);
  ASSERT_TRUE(collector.stop());
  return true;
}

static bool test_collector_poll() {
  ConfigCollector collector;
  ASSERT_TRUE(collector.start());
  ASSERT_TRUE(collector.poll());
  ASSERT_TRUE(collector.stop());
  return true;
}

static bool test_collector_events_emitted() {
  ConfigCollector collector;
  ASSERT_TRUE(collector.start());
  std::size_t before = collector.eventsEmitted();
  collector.poll();
  std::size_t after = collector.eventsEmitted();
  ASSERT_TRUE(after >= before);
  ASSERT_TRUE(collector.stop());
  return true;
}

static bool test_collector_callback() {
  ConfigCollector collector;
  int event_count = 0;

  collector.setCallback([&](const ConfigChange&, ConfigDetectionOrigin) {
    event_count++;
  });

  ASSERT_TRUE(collector.start());
  ASSERT_TRUE(event_count > 0);
  ASSERT_TRUE(collector.stop());
  return true;
}

static bool test_collector_permission_not_running() {
  ConfigCollector collector;
  ASSERT_FALSE(collector.poll());
  ASSERT_EQ(collector.changesTracked(), static_cast<std::size_t>(0));
  return true;
}

static bool test_scope_filtering() {
  ConfigCollector collector;
  ConfigScope scope;
  scope.areas = {ConfigArea::Security};
  scope.min_severity = ConfigChangeSeverity::Critical;
  collector.addScope(scope);

  int event_count = 0;
  collector.setCallback([&](const ConfigChange& change, ConfigDetectionOrigin) {
    event_count++;
    ASSERT_TRUE(change.area == ConfigArea::Security);
    ASSERT_TRUE(change.severity == ConfigChangeSeverity::Critical);
  });

  ASSERT_TRUE(collector.start());
  ASSERT_TRUE(collector.stop());
  return true;
}

static bool test_scope_clear() {
  ConfigCollector collector;
  ConfigScope scope;
  scope.areas = {ConfigArea::Security};
  collector.addScope(scope);
  collector.clearScopes();

  int event_count = 0;
  collector.setCallback([&](const ConfigChange&, ConfigDetectionOrigin) {
    event_count++;
  });

  ASSERT_TRUE(collector.start());
  ASSERT_TRUE(event_count > 0);
  ASSERT_TRUE(collector.stop());
  return true;
}

static bool test_config_defaults() {
  ConfigCollectorConfig cfg;
  ASSERT_TRUE(cfg.track_security);
  ASSERT_TRUE(cfg.track_firewall);
  ASSERT_TRUE(cfg.track_startup);
  ASSERT_TRUE(cfg.track_services);
  ASSERT_TRUE(cfg.track_drivers);
  ASSERT_TRUE(cfg.track_policies);
  ASSERT_TRUE(cfg.track_system_config);
  ASSERT_FALSE(cfg.include_ignored);
  return true;
}

static bool test_config_set() {
  ConfigCollectorConfig cfg;
  cfg.track_security = false;
  cfg.min_severity = ConfigChangeSeverity::Critical;
  ConfigCollector collector(cfg);
  ASSERT_FALSE(collector.config().track_security);
  ASSERT_EQ(collector.config().min_severity, ConfigChangeSeverity::Critical);
  return true;
}

static bool test_double_start() {
  ConfigCollector collector;
  ASSERT_TRUE(collector.start());
  ASSERT_FALSE(collector.start());
  ASSERT_TRUE(collector.stop());
  ASSERT_FALSE(collector.stop());
  return true;
}

static bool test_duplicate_suppression() {
  ConfigCollector collector;
  int event_count = 0;
  collector.setCallback([&](const ConfigChange&, ConfigDetectionOrigin) {
    event_count++;
  });
  ASSERT_TRUE(collector.start());
  int first = event_count;
  collector.poll();
  ASSERT_EQ(event_count, first);
  ASSERT_TRUE(collector.stop());
  return true;
}

static bool test_critical_change() {
  ConfigCollectorConfig cfg;
  cfg.track_firewall = true;
  ConfigCollector collector(cfg);

  bool got_critical = false;
  collector.setCallback([&](const ConfigChange& change, ConfigDetectionOrigin) {
    if (change.severity == ConfigChangeSeverity::Critical) got_critical = true;
  });

  ASSERT_TRUE(collector.start());
  ASSERT_TRUE(got_critical);
  ASSERT_TRUE(collector.stop());
  return true;
}

static bool test_important_change() {
  ConfigCollectorConfig cfg;
  cfg.track_startup = true;
  ConfigCollector collector(cfg);

  bool got_important = false;
  collector.setCallback([&](const ConfigChange& change, ConfigDetectionOrigin) {
    if (change.severity == ConfigChangeSeverity::Important) got_important = true;
  });

  ASSERT_TRUE(collector.start());
  ASSERT_TRUE(got_important);
  ASSERT_TRUE(collector.stop());
  return true;
}

static bool test_normal_change() {
  ConfigCollectorConfig cfg;
  cfg.track_system_config = true;
  ConfigCollector collector(cfg);

  bool got_normal = false;
  collector.setCallback([&](const ConfigChange& change, ConfigDetectionOrigin) {
    if (change.severity == ConfigChangeSeverity::Normal) got_normal = true;
  });

  ASSERT_TRUE(collector.start());
  ASSERT_TRUE(got_normal);
  ASSERT_TRUE(collector.stop());
  return true;
}

static bool test_ignored_filtering() {
  ConfigCollectorConfig cfg;
  cfg.include_ignored = false;
  ConfigCollector collector(cfg);

  bool got_ignored = false;
  collector.setCallback([&](const ConfigChange& change, ConfigDetectionOrigin) {
    if (change.severity == ConfigChangeSeverity::Ignored) got_ignored = true;
  });

  ASSERT_TRUE(collector.start());
  ASSERT_FALSE(got_ignored);
  ASSERT_TRUE(collector.stop());
  return true;
}

#ifndef MONIX_KERNEL_BUILD
int main() {
  std::cerr << "=== MONIX Configuration Collector Tests ===\n\n";

  RUN_TEST(severity_names);
  RUN_TEST(severity_actions);
  RUN_TEST(area_names);
  RUN_TEST(area_from_name);
  RUN_TEST(change_kind_names);
  RUN_TEST(detection_origin_names);
  RUN_TEST(scope_area_enabled);
  RUN_TEST(scope_severity_visible);
  RUN_TEST(scope_matches);
  RUN_TEST(change_validity);
  RUN_TEST(change_severity_checks);
  RUN_TEST(change_summary);
  RUN_TEST(collector_lifecycle);
  RUN_TEST(collector_snapshot);
  RUN_TEST(collector_changes_tracked);
  RUN_TEST(collector_poll);
  RUN_TEST(collector_events_emitted);
  RUN_TEST(collector_callback);
  RUN_TEST(collector_permission_not_running);
  RUN_TEST(scope_filtering);
  RUN_TEST(scope_clear);
  RUN_TEST(config_defaults);
  RUN_TEST(config_set);
  RUN_TEST(double_start);
  RUN_TEST(duplicate_suppression);
  RUN_TEST(critical_change);
  RUN_TEST(important_change);
  RUN_TEST(normal_change);
  RUN_TEST(ignored_filtering);

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
  {"Severity names", test_severity_names},
  {"Severity actions", test_severity_actions},
  {"Area names", test_area_names},
  {"Area from name", test_area_from_name},
  {"Change kind names", test_change_kind_names},
  {"Detection origin names", test_detection_origin_names},
  {"Scope area enabled", test_scope_area_enabled},
  {"Scope severity visible", test_scope_severity_visible},
  {"Scope matches", test_scope_matches},
  {"Change validity", test_change_validity},
  {"Change severity checks", test_change_severity_checks},
  {"Change summary", test_change_summary},
  {"Collector lifecycle", test_collector_lifecycle},
  {"Collector snapshot", test_collector_snapshot},
  {"Collector changes tracked", test_collector_changes_tracked},
  {"Collector poll", test_collector_poll},
  {"Collector events emitted", test_collector_events_emitted},
  {"Collector callback", test_collector_callback},
  {"Collector permission not running", test_collector_permission_not_running},
  {"Scope filtering", test_scope_filtering},
  {"Scope clear", test_scope_clear},
  {"Config defaults", test_config_defaults},
  {"Config set", test_config_set},
  {"Double start", test_double_start},
  {"Duplicate suppression", test_duplicate_suppression},
  {"Critical change", test_critical_change},
  {"Important change", test_important_change},
  {"Normal change", test_normal_change},
  {"Ignored filtering", test_ignored_filtering},
};

const KBoolTestEntry* GetKBoolTests_ConfigCollector() { return s_kbooltests; }
std::size_t GetKBoolTestCount_ConfigCollector() { return sizeof(s_kbooltests) / sizeof(s_kbooltests[0]); }
#endif
