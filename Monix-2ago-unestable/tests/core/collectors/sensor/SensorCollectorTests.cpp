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

#include "SensorCollector.hpp"

using namespace monix::collectors::sensor;

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

static bool test_sensor_kind_names() {
  ASSERT_EQ(std::string(SensorKindName(SensorKind::CpuUsage)), "CpuUsage");
  ASSERT_EQ(std::string(SensorKindName(SensorKind::GpuUsage)), "GpuUsage");
  ASSERT_EQ(std::string(SensorKindName(SensorKind::GpuTemperature)), "GpuTemperature");
  ASSERT_EQ(std::string(SensorKindName(SensorKind::RamUsage)), "RamUsage");
  ASSERT_EQ(std::string(SensorKindName(SensorKind::Temperature)), "Temperature");
  ASSERT_EQ(std::string(SensorKindName(SensorKind::DiskUsage)), "DiskUsage");
  ASSERT_EQ(std::string(SensorKindName(SensorKind::NetworkThroughput)), "NetworkThroughput");
  ASSERT_EQ(std::string(SensorKindName(SensorKind::BatteryLevel)), "BatteryLevel");
  ASSERT_EQ(std::string(SensorKindName(SensorKind::PowerDraw)), "PowerDraw");
  return true;
}

static bool test_sensor_kind_units() {
  ASSERT_EQ(SensorKindUnit(SensorKind::CpuUsage), "%");
  ASSERT_EQ(SensorKindUnit(SensorKind::Temperature), "°C");
  ASSERT_EQ(SensorKindUnit(SensorKind::DiskIO), "MB/s");
  ASSERT_EQ(SensorKindUnit(SensorKind::PowerDraw), "W");
  return true;
}

static bool test_state_names() {
  ASSERT_EQ(std::string(SensorStateName(SensorState::Normal)), "Normal");
  ASSERT_EQ(std::string(SensorStateName(SensorState::Warning)), "Warning");
  ASSERT_EQ(std::string(SensorStateName(SensorState::Critical)), "Critical");
  ASSERT_EQ(std::string(SensorStateName(SensorState::Unknown)), "Unknown");
  return true;
}

static bool test_event_kind_names() {
  ASSERT_EQ(std::string(SensorEventKindName(SensorEventKind::Warning)), "Warning");
  ASSERT_EQ(std::string(SensorEventKindName(SensorEventKind::Critical)), "Critical");
  ASSERT_EQ(std::string(SensorEventKindName(SensorEventKind::Clear)), "Clear");
  return true;
}

static bool test_event_kind_actions() {
  ASSERT_EQ(SensorEventKindAction(SensorEventKind::Warning), "sensor.warning");
  ASSERT_EQ(SensorEventKindAction(SensorEventKind::Critical), "sensor.critical");
  ASSERT_EQ(SensorEventKindAction(SensorEventKind::Clear), "sensor.clear");
  return true;
}

static bool test_detection_origin_names() {
  ASSERT_EQ(std::string(SensorDetectionOriginName(SensorDetectionOrigin::WMI)), "WMI");
  ASSERT_EQ(std::string(SensorDetectionOriginName(SensorDetectionOrigin::PerformanceCounter)), "PerformanceCounter");
  ASSERT_EQ(std::string(SensorDetectionOriginName(SensorDetectionOrigin::Polling)), "Polling");
  ASSERT_EQ(std::string(SensorDetectionOriginName(SensorDetectionOrigin::Manual)), "Manual");
  return true;
}

static bool test_threshold_above_warning() {
  SensorThreshold t;
  t.warning_threshold = 70.0;
  ASSERT_TRUE(t.isAboveWarning(70.0));
  ASSERT_TRUE(t.isAboveWarning(75.0));
  ASSERT_FALSE(t.isAboveWarning(69.9));
  return true;
}

static bool test_threshold_above_critical() {
  SensorThreshold t;
  t.critical_threshold = 90.0;
  ASSERT_TRUE(t.isAboveCritical(90.0));
  ASSERT_TRUE(t.isAboveCritical(95.0));
  ASSERT_FALSE(t.isAboveCritical(89.9));
  return true;
}

static bool test_threshold_below_clear() {
  SensorThreshold t;
  t.warning_threshold = 70.0;
  t.critical_threshold = 90.0;
  t.hysteresis = 5.0;

  ASSERT_TRUE(t.isBelowClear(64.0, SensorState::Warning));
  ASSERT_FALSE(t.isBelowClear(66.0, SensorState::Warning));
  ASSERT_FALSE(t.isBelowClear(64.0, SensorState::Normal));

  ASSERT_TRUE(t.isBelowClear(84.0, SensorState::Critical));
  ASSERT_FALSE(t.isBelowClear(86.0, SensorState::Critical));
  return true;
}

static bool test_threshold_has_changed() {
  SensorThreshold t;
  t.change_threshold = 2.0;
  ASSERT_TRUE(t.hasChanged(50.0, 53.0));
  ASSERT_TRUE(t.hasChanged(50.0, 47.0));
  ASSERT_FALSE(t.hasChanged(50.0, 51.0));
  ASSERT_FALSE(t.hasChanged(50.0, 50.0));
  return true;
}

static bool test_threshold_no_change_threshold() {
  SensorThreshold t;
  t.change_threshold = 0.0;
  ASSERT_TRUE(t.hasChanged(50.0, 50.0));
  return true;
}

static bool test_reading_validity() {
  SensorReading r;
  ASSERT_FALSE(r.isValid());
  r.name = "CPU0";
  ASSERT_TRUE(r.isValid());
  return true;
}

static bool test_reading_summary() {
  SensorReading r;
  r.name = "CPU0";
  r.label = "Total";
  r.value = 75.0;
  r.unit = "%";
  r.state = SensorState::Warning;
  std::string s = r.summary();
  ASSERT_TRUE(s.find("CPU0") != std::string::npos);
  ASSERT_TRUE(s.find("Warning") != std::string::npos);
  return true;
}

static bool test_event_validity() {
  SensorEvent e;
  ASSERT_FALSE(e.isValid());
  e.name = "CPU0";
  ASSERT_TRUE(e.isValid());
  return true;
}

static bool test_event_checks() {
  SensorEvent e;
  e.event_kind = SensorEventKind::Warning;
  ASSERT_TRUE(e.isWarning());
  ASSERT_FALSE(e.isCritical());
  ASSERT_FALSE(e.isClear());

  e.event_kind = SensorEventKind::Critical;
  ASSERT_FALSE(e.isWarning());
  ASSERT_TRUE(e.isCritical());
  ASSERT_FALSE(e.isClear());

  e.event_kind = SensorEventKind::Clear;
  ASSERT_FALSE(e.isWarning());
  ASSERT_FALSE(e.isCritical());
  ASSERT_TRUE(e.isClear());
  return true;
}

static bool test_collector_lifecycle() {
  SensorCollectorConfig cfg;
  SensorCollector collector(cfg);
  ASSERT_FALSE(collector.isRunning());
  ASSERT_TRUE(collector.start());
  ASSERT_TRUE(collector.isRunning());
  ASSERT_FALSE(collector.start());
  ASSERT_TRUE(collector.stop());
  ASSERT_FALSE(collector.isRunning());
  ASSERT_FALSE(collector.stop());
  return true;
}

static bool test_collector_permission_not_running() {
  SensorCollector collector;
  SensorReading r;
  r.kind = SensorKind::CpuUsage;
  r.name = "CPU0";
  r.value = 50.0;
  ASSERT_FALSE(collector.pushReading(r));
  ASSERT_EQ(collector.sensorsTracked(), static_cast<std::size_t>(0));
  return true;
}

static bool test_collector_threshold_warning() {
  SensorCollectorConfig cfg;
  cfg.cpu_threshold.warning_threshold = 70.0;
  cfg.cpu_threshold.change_threshold = 0.0;
  SensorCollector collector(cfg);

  bool got_warning = false;
  collector.setCallback([&](const SensorEvent& e, SensorDetectionOrigin) {
    if (e.event_kind == SensorEventKind::Warning) got_warning = true;
  });

  collector.start();
  SensorReading r;
  r.kind = SensorKind::CpuUsage;
  r.name = "CPU0";
  r.value = 75.0;
  collector.pushReading(r);
  ASSERT_TRUE(got_warning);
  collector.stop();
  return true;
}

static bool test_collector_threshold_critical() {
  SensorCollectorConfig cfg;
  cfg.cpu_threshold.critical_threshold = 90.0;
  cfg.cpu_threshold.change_threshold = 0.0;
  SensorCollector collector(cfg);

  bool got_critical = false;
  collector.setCallback([&](const SensorEvent& e, SensorDetectionOrigin) {
    if (e.event_kind == SensorEventKind::Critical) got_critical = true;
  });

  collector.start();
  SensorReading r;
  r.kind = SensorKind::CpuUsage;
  r.name = "CPU0";
  r.value = 95.0;
  collector.pushReading(r);
  ASSERT_TRUE(got_critical);
  collector.stop();
  return true;
}

static bool test_collector_clear() {
  SensorCollectorConfig cfg;
  cfg.cpu_threshold.warning_threshold = 70.0;
  cfg.cpu_threshold.hysteresis = 5.0;
  cfg.cpu_threshold.change_threshold = 0.0;
  SensorCollector collector(cfg);

  bool got_warning = false;
  bool got_clear = false;
  collector.setCallback([&](const SensorEvent& e, SensorDetectionOrigin) {
    if (e.event_kind == SensorEventKind::Warning) got_warning = true;
    if (e.event_kind == SensorEventKind::Clear) got_clear = true;
  });

  collector.start();

  SensorReading r;
  r.kind = SensorKind::CpuUsage;
  r.name = "CPU0";

  r.value = 75.0;
  collector.pushReading(r);
  ASSERT_TRUE(got_warning);

  r.value = 64.0;
  collector.pushReading(r);
  ASSERT_TRUE(got_clear);

  collector.stop();
  return true;
}

static bool test_hysteresis() {
  SensorCollectorConfig cfg;
  cfg.temp_threshold.warning_threshold = 70.0;
  cfg.temp_threshold.hysteresis = 5.0;
  cfg.temp_threshold.change_threshold = 0.0;
  SensorCollector collector(cfg);

  int warning_count = 0;
  int clear_count = 0;
  collector.setCallback([&](const SensorEvent& e, SensorDetectionOrigin) {
    if (e.event_kind == SensorEventKind::Warning) warning_count++;
    if (e.event_kind == SensorEventKind::Clear) clear_count++;
  });

  collector.start();

  SensorReading r;
  r.kind = SensorKind::Temperature;
  r.name = "CPU";

  r.value = 75.0;
  collector.pushReading(r);
  ASSERT_EQ(warning_count, 1);

  r.value = 68.0;
  collector.pushReading(r);
  ASSERT_EQ(clear_count, 0);

  r.value = 64.0;
  collector.pushReading(r);
  ASSERT_EQ(clear_count, 1);

  collector.stop();
  return true;
}

static bool test_rapid_fluctuations() {
  SensorCollectorConfig cfg;
  cfg.cpu_threshold.warning_threshold = 70.0;
  cfg.cpu_threshold.hysteresis = 5.0;
  cfg.cpu_threshold.change_threshold = 0.0;
  SensorCollector collector(cfg);

  int warning_count = 0;
  int clear_count = 0;
  collector.setCallback([&](const SensorEvent& e, SensorDetectionOrigin) {
    if (e.event_kind == SensorEventKind::Warning) warning_count++;
    if (e.event_kind == SensorEventKind::Clear) clear_count++;
  });

  collector.start();

  SensorReading r;
  r.kind = SensorKind::CpuUsage;
  r.name = "CPU0";

  double values[] = {75.0, 68.0, 73.0, 69.0, 71.0, 67.0, 74.0, 66.0, 72.0, 68.0};
  for (double v : values) {
    r.value = v;
    collector.pushReading(r);
  }

  ASSERT_EQ(warning_count, 1);
  ASSERT_EQ(clear_count, 0);

  collector.stop();
  return true;
}

static bool test_sampling_interval() {
  SensorCollectorConfig cfg;
  cfg.sampling_interval_ms = std::chrono::milliseconds(100);
  SensorCollector collector(cfg);
  ASSERT_EQ(collector.config().sampling_interval_ms.count(), 100);
  return true;
}

static bool test_aggregation() {
  SensorCollectorConfig cfg;
  cfg.cpu_threshold.warning_threshold = 70.0;
  cfg.cpu_threshold.change_threshold = 5.0;
  SensorCollector collector(cfg);

  int event_count = 0;
  collector.setCallback([&](const SensorEvent&, SensorDetectionOrigin) {
    event_count++;
  });

  collector.start();

  SensorReading r;
  r.kind = SensorKind::CpuUsage;
  r.name = "CPU0";
  r.value = 75.0;
  collector.pushReading(r);
  int first = event_count;

  r.value = 76.0;
  collector.pushReading(r);
  ASSERT_EQ(event_count, first);

  r.value = 82.0;
  collector.pushReading(r);
  ASSERT_TRUE(event_count > first);

  collector.stop();
  return true;
}

static bool test_collector_snapshot() {
  SensorCollectorConfig cfg;
  cfg.cpu_threshold.warning_threshold = 70.0;
  cfg.cpu_threshold.change_threshold = 0.0;
  SensorCollector collector(cfg);
  collector.start();

  SensorReading r;
  r.kind = SensorKind::CpuUsage;
  r.name = "CPU0";
  r.value = 50.0;
  collector.pushReading(r);

  auto snap = collector.snapshot();
  ASSERT_TRUE(snap.size() > 0);
  ASSERT_TRUE(collector.sensorsTracked() > 0);

  collector.stop();
  return true;
}

static bool test_collector_poll() {
  SensorCollector collector;
  collector.start();
  ASSERT_TRUE(collector.poll());
  collector.stop();
  return true;
}

static bool test_set_threshold() {
  SensorCollector collector;
  SensorThreshold t;
  t.warning_threshold = 80.0;
  collector.setThreshold(SensorKind::CpuUsage, t);
  ASSERT_EQ(collector.config().cpu_threshold.warning_threshold, 80.0);
  return true;
}

static bool test_config_defaults() {
  SensorCollectorConfig cfg;
  ASSERT_EQ(cfg.sampling_interval_ms.count(), 5000);
  ASSERT_EQ(cfg.dedup_window_ms.count(), 2000);
  return true;
}

static bool test_double_start() {
  SensorCollector collector;
  ASSERT_TRUE(collector.start());
  ASSERT_FALSE(collector.start());
  ASSERT_TRUE(collector.stop());
  ASSERT_FALSE(collector.stop());
  return true;
}

#ifndef MONIX_KERNEL_BUILD
int main() {
  std::cerr << "=== MONIX Sensor Collector Tests ===\n\n";

  RUN_TEST(sensor_kind_names);
  RUN_TEST(sensor_kind_units);
  RUN_TEST(state_names);
  RUN_TEST(event_kind_names);
  RUN_TEST(event_kind_actions);
  RUN_TEST(detection_origin_names);
  RUN_TEST(threshold_above_warning);
  RUN_TEST(threshold_above_critical);
  RUN_TEST(threshold_below_clear);
  RUN_TEST(threshold_has_changed);
  RUN_TEST(threshold_no_change_threshold);
  RUN_TEST(reading_validity);
  RUN_TEST(reading_summary);
  RUN_TEST(event_validity);
  RUN_TEST(event_checks);
  RUN_TEST(collector_lifecycle);
  RUN_TEST(collector_permission_not_running);
  RUN_TEST(collector_threshold_warning);
  RUN_TEST(collector_threshold_critical);
  RUN_TEST(collector_clear);
  RUN_TEST(hysteresis);
  RUN_TEST(rapid_fluctuations);
  RUN_TEST(sampling_interval);
  RUN_TEST(aggregation);
  RUN_TEST(collector_snapshot);
  RUN_TEST(collector_poll);
  RUN_TEST(set_threshold);
  RUN_TEST(config_defaults);
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
  {"Sensor kind names", test_sensor_kind_names},
  {"Sensor kind units", test_sensor_kind_units},
  {"State names", test_state_names},
  {"Event kind names", test_event_kind_names},
  {"Event kind actions", test_event_kind_actions},
  {"Detection origin names", test_detection_origin_names},
  {"Threshold above warning", test_threshold_above_warning},
  {"Threshold above critical", test_threshold_above_critical},
  {"Threshold below clear", test_threshold_below_clear},
  {"Threshold has changed", test_threshold_has_changed},
  {"Threshold no change", test_threshold_no_change_threshold},
  {"Reading validity", test_reading_validity},
  {"Reading summary", test_reading_summary},
  {"Event validity", test_event_validity},
  {"Event checks", test_event_checks},
  {"Collector lifecycle", test_collector_lifecycle},
  {"Collector permission not running", test_collector_permission_not_running},
  {"Collector threshold warning", test_collector_threshold_warning},
  {"Collector threshold critical", test_collector_threshold_critical},
  {"Collector clear", test_collector_clear},
  {"Hysteresis", test_hysteresis},
  {"Rapid fluctuations", test_rapid_fluctuations},
  {"Sampling interval", test_sampling_interval},
  {"Aggregation", test_aggregation},
  {"Collector snapshot", test_collector_snapshot},
  {"Collector poll", test_collector_poll},
  {"Set threshold", test_set_threshold},
  {"Config defaults", test_config_defaults},
  {"Double start", test_double_start},
};

const KBoolTestEntry* GetKBoolTests_SensorCollector() { return s_kbooltests; }
std::size_t GetKBoolTestCount_SensorCollector() { return sizeof(s_kbooltests) / sizeof(s_kbooltests[0]); }
#endif
