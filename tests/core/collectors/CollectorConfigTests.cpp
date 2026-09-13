#include "../../core/collectors/CollectorConfig.hpp"

#include <cassert>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>
#include <atomic>

using namespace monix::collectors;

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

// ==================== Enum Name Tests ====================

static void test_mode_names() {
  TEST("CollectorMode: all names");
  ASSERT_EQ(std::string(CollectorModeName(CollectorMode::Passive)), "Passive");
  ASSERT_EQ(std::string(CollectorModeName(CollectorMode::Active)), "Active");
  ASSERT_EQ(std::string(CollectorModeName(CollectorMode::Hybrid)), "Hybrid");
  PASS();
}

static void test_mode_from_string() {
  TEST("CollectorMode: fromString roundtrip");
  ASSERT_EQ(CollectorModeFromString("Passive"), CollectorMode::Passive);
  ASSERT_EQ(CollectorModeFromString("Active"), CollectorMode::Active);
  ASSERT_EQ(CollectorModeFromString("Hybrid"), CollectorMode::Hybrid);
  ASSERT_EQ(CollectorModeFromString("invalid"), CollectorMode::Passive);
  PASS();
}

static void test_log_level_names() {
  TEST("CollectorLogLevel: all names");
  ASSERT_EQ(std::string(CollectorLogLevelName(CollectorLogLevel::Silent)), "Silent");
  ASSERT_EQ(std::string(CollectorLogLevelName(CollectorLogLevel::Error)), "Error");
  ASSERT_EQ(std::string(CollectorLogLevelName(CollectorLogLevel::Warning)), "Warning");
  ASSERT_EQ(std::string(CollectorLogLevelName(CollectorLogLevel::Info)), "Info");
  ASSERT_EQ(std::string(CollectorLogLevelName(CollectorLogLevel::Debug)), "Debug");
  ASSERT_EQ(std::string(CollectorLogLevelName(CollectorLogLevel::Trace)), "Trace");
  PASS();
}

static void test_log_level_from_string() {
  TEST("CollectorLogLevel: fromString roundtrip");
  ASSERT_EQ(CollectorLogLevelFromString("Silent"), CollectorLogLevel::Silent);
  ASSERT_EQ(CollectorLogLevelFromString("Error"), CollectorLogLevel::Error);
  ASSERT_EQ(CollectorLogLevelFromString("Warning"), CollectorLogLevel::Warning);
  ASSERT_EQ(CollectorLogLevelFromString("Info"), CollectorLogLevel::Info);
  ASSERT_EQ(CollectorLogLevelFromString("Debug"), CollectorLogLevel::Debug);
  ASSERT_EQ(CollectorLogLevelFromString("Trace"), CollectorLogLevel::Trace);
  ASSERT_EQ(CollectorLogLevelFromString("invalid"), CollectorLogLevel::Info);
  PASS();
}

static void test_field_names() {
  TEST("CollectorConfigField: all names non-null");
  for (int i = 1; i <= 9; ++i) {
    const char* name = CollectorConfigFieldName(static_cast<CollectorConfigField>(i));
    ASSERT_TRUE(name != nullptr);
    ASSERT_TRUE(std::string(name).size() > 0);
  }
  PASS();
}

static void test_field_type_names() {
  TEST("ConfigFieldType: all names");
  ASSERT_EQ(std::string(ConfigFieldTypeName(ConfigFieldType::Boolean)), "Boolean");
  ASSERT_EQ(std::string(ConfigFieldTypeName(ConfigFieldType::Int64)), "Int64");
  ASSERT_EQ(std::string(ConfigFieldTypeName(ConfigFieldType::UInt64)), "UInt64");
  ASSERT_EQ(std::string(ConfigFieldTypeName(ConfigFieldType::Double)), "Double");
  ASSERT_EQ(std::string(ConfigFieldTypeName(ConfigFieldType::String)), "String");
  PASS();
}

// ==================== Default Values Tests ====================

static void test_defaults_all_present() {
  TEST("Defaults: all 9 fields present");
  CollectorConfig cfg;
  ASSERT_EQ(cfg.fieldCount(), 9u);
  PASS();
}

static void test_defaults_enabled() {
  TEST("Defaults: enabled = true");
  CollectorConfig cfg;
  ASSERT_TRUE(cfg.enabled());
  ASSERT_TRUE(cfg.getBool(CollectorConfigField::Enabled));
  PASS();
}

static void test_defaults_sampling_interval() {
  TEST("Defaults: sampling_interval_ms = 1000");
  CollectorConfig cfg;
  ASSERT_EQ(cfg.samplingIntervalMs(), 1000);
  PASS();
}

static void test_defaults_max_events_per_second() {
  TEST("Defaults: max_events_per_second = 1000");
  CollectorConfig cfg;
  ASSERT_EQ(cfg.maxEventsPerSecond(), 1000u);
  PASS();
}

static void test_defaults_queue_limit() {
  TEST("Defaults: queue_limit = 100000");
  CollectorConfig cfg;
  ASSERT_EQ(cfg.queueLimit(), 100000u);
  PASS();
}

static void test_defaults_auto_restart() {
  TEST("Defaults: auto_restart = true");
  CollectorConfig cfg;
  ASSERT_TRUE(cfg.autoRestart());
  PASS();
}

static void test_defaults_max_restart_attempts() {
  TEST("Defaults: max_restart_attempts = 3");
  CollectorConfig cfg;
  ASSERT_EQ(cfg.maxRestartAttempts(), 3u);
  PASS();
}

static void test_defaults_restart_backoff() {
  TEST("Defaults: restart_backoff_ms = 5000");
  CollectorConfig cfg;
  ASSERT_EQ(cfg.restartBackoffMs(), 5000u);
  PASS();
}

static void test_defaults_mode() {
  TEST("Defaults: mode = Passive");
  CollectorConfig cfg;
  ASSERT_EQ(cfg.mode(), CollectorMode::Passive);
  PASS();
}

static void test_defaults_log_level() {
  TEST("Defaults: log_level = Info");
  CollectorConfig cfg;
  ASSERT_EQ(cfg.logLevel(), CollectorLogLevel::Info);
  PASS();
}

// ==================== Override Tests ====================

static void test_override_enabled() {
  TEST("Override: enabled = false");
  CollectorConfig cfg;
  cfg.set(CollectorConfigField::Enabled, false);
  ASSERT_FALSE(cfg.enabled());
  PASS();
}

static void test_override_sampling_interval() {
  TEST("Override: sampling_interval_ms = 500");
  CollectorConfig cfg;
  cfg.set(CollectorConfigField::SamplingIntervalMs, std::int64_t(500));
  ASSERT_EQ(cfg.samplingIntervalMs(), 500);
  PASS();
}

static void test_override_max_events() {
  TEST("Override: max_events_per_second = 5000");
  CollectorConfig cfg;
  cfg.set(CollectorConfigField::MaxEventsPerSecond, std::uint64_t(5000));
  ASSERT_EQ(cfg.maxEventsPerSecond(), 5000u);
  PASS();
}

static void test_override_queue_limit() {
  TEST("Override: queue_limit = 500000");
  CollectorConfig cfg;
  cfg.set(CollectorConfigField::QueueLimit, std::uint64_t(500000));
  ASSERT_EQ(cfg.queueLimit(), 500000u);
  PASS();
}

static void test_override_auto_restart() {
  TEST("Override: auto_restart = false");
  CollectorConfig cfg;
  cfg.set(CollectorConfigField::AutoRestart, false);
  ASSERT_FALSE(cfg.autoRestart());
  PASS();
}

static void test_override_max_restart_attempts() {
  TEST("Override: max_restart_attempts = 10");
  CollectorConfig cfg;
  cfg.set(CollectorConfigField::MaxRestartAttempts, std::int64_t(10));
  ASSERT_EQ(cfg.maxRestartAttempts(), 10u);
  PASS();
}

static void test_override_restart_backoff() {
  TEST("Override: restart_backoff_ms = 30000");
  CollectorConfig cfg;
  cfg.set(CollectorConfigField::RestartBackoffMs, std::int64_t(30000));
  ASSERT_EQ(cfg.restartBackoffMs(), 30000u);
  PASS();
}

static void test_override_mode() {
  TEST("Override: mode = Active");
  CollectorConfig cfg;
  cfg.set(CollectorConfigField::Mode, std::int64_t(1));
  ASSERT_EQ(cfg.mode(), CollectorMode::Active);
  PASS();
}

static void test_override_log_level() {
  TEST("Override: log_level = Debug");
  CollectorConfig cfg;
  cfg.set(CollectorConfigField::LogLevel, std::int64_t(4));
  ASSERT_EQ(cfg.logLevel(), CollectorLogLevel::Debug);
  PASS();
}

// ==================== Validation Tests ====================

static void test_valid_config_passes() {
  TEST("Validation: default config is valid");
  CollectorConfig cfg;
  ASSERT_TRUE(cfg.isValid());
  ASSERT_TRUE(cfg.validate().empty());
  PASS();
}

static void test_validation_sampling_too_low() {
  TEST("Validation: sampling_interval_ms below min is invalid");
  CollectorConfig cfg;
  cfg.set(CollectorConfigField::SamplingIntervalMs, std::int64_t(-1));
  ASSERT_FALSE(cfg.isValid());
  auto issues = cfg.validate();
  ASSERT_TRUE(issues.size() > 0);
  ASSERT_EQ(issues[0].field, CollectorConfigField::SamplingIntervalMs);
  PASS();
}

static void test_validation_sampling_too_high() {
  TEST("Validation: sampling_interval_ms above max is invalid");
  CollectorConfig cfg;
  cfg.set(CollectorConfigField::SamplingIntervalMs, std::int64_t(999999999));
  ASSERT_FALSE(cfg.isValid());
  PASS();
}

static void test_validation_max_events_zero() {
  TEST("Validation: max_events_per_second = 0 below min is invalid");
  CollectorConfig cfg;
  cfg.set(CollectorConfigField::MaxEventsPerSecond, std::uint64_t(0));
  ASSERT_FALSE(cfg.isValid());
  PASS();
}

static void test_validation_queue_limit_below_min() {
  TEST("Validation: queue_limit below min is invalid");
  CollectorConfig cfg;
  cfg.set(CollectorConfigField::QueueLimit, std::uint64_t(10));
  ASSERT_FALSE(cfg.isValid());
  PASS();
}

static void test_validation_max_restart_attempts_negative() {
  TEST("Validation: max_restart_attempts negative is invalid");
  CollectorConfig cfg;
  cfg.set(CollectorConfigField::MaxRestartAttempts, std::int64_t(-5));
  ASSERT_FALSE(cfg.isValid());
  PASS();
}

static void test_validation_restart_backoff_too_low() {
  TEST("Validation: restart_backoff_ms below min is invalid");
  CollectorConfig cfg;
  cfg.set(CollectorConfigField::RestartBackoffMs, std::int64_t(10));
  ASSERT_FALSE(cfg.isValid());
  PASS();
}

static void test_validation_boundary_values() {
  TEST("Validation: boundary values accepted");
  CollectorConfig cfg;
  cfg.set(CollectorConfigField::SamplingIntervalMs, std::int64_t(0));
  ASSERT_TRUE(cfg.isValid());

  cfg.set(CollectorConfigField::SamplingIntervalMs, std::int64_t(86400000));
  ASSERT_TRUE(cfg.isValid());

  cfg.set(CollectorConfigField::MaxEventsPerSecond, std::uint64_t(1));
  ASSERT_TRUE(cfg.isValid());

  cfg.set(CollectorConfigField::MaxEventsPerSecond, std::uint64_t(1000000));
  ASSERT_TRUE(cfg.isValid());
  PASS();
}

// ==================== Merge Tests ====================

static void test_merge_overrides() {
  TEST("Merge: override only specified fields");
  CollectorConfig base;
  CollectorConfig override;
  override.set(CollectorConfigField::Enabled, false);
  override.set(CollectorConfigField::SamplingIntervalMs, std::int64_t(200));

  base.merge(override);
  ASSERT_FALSE(base.enabled());
  ASSERT_EQ(base.samplingIntervalMs(), 200);
  ASSERT_EQ(base.maxEventsPerSecond(), 1000u);
  PASS();
}

static void test_merge_empty() {
  TEST("Merge: empty config preserves all defaults");
  CollectorConfig base;
  CollectorConfig empty;
  base.merge(empty);
  ASSERT_TRUE(base.isValid());
  ASSERT_EQ(base.fieldCount(), 9u);
  PASS();
}

// ==================== Reset Tests ====================

static void test_reset_restores_defaults() {
  TEST("Reset: restores all defaults");
  CollectorConfig cfg;
  cfg.set(CollectorConfigField::Enabled, false);
  cfg.set(CollectorConfigField::SamplingIntervalMs, std::int64_t(999));
  cfg.reset();
  ASSERT_TRUE(cfg.enabled());
  ASSERT_EQ(cfg.samplingIntervalMs(), 1000);
  PASS();
}

// ==================== Remove Tests ====================

static void test_remove_field() {
  TEST("Remove: removed field falls back to default");
  CollectorConfig cfg;
  cfg.set(CollectorConfigField::SamplingIntervalMs, std::int64_t(500));
  ASSERT_EQ(cfg.samplingIntervalMs(), 500);
  cfg.remove(CollectorConfigField::SamplingIntervalMs);
  ASSERT_FALSE(cfg.has(CollectorConfigField::SamplingIntervalMs));
  ASSERT_EQ(cfg.samplingIntervalMs(), 1000);
  PASS();
}

// ==================== Has Tests ====================

static void test_has_field() {
  TEST("Has: tracks explicitly set fields");
  CollectorConfig cfg;
  ASSERT_TRUE(cfg.has(CollectorConfigField::SamplingIntervalMs));
  cfg.remove(CollectorConfigField::SamplingIntervalMs);
  ASSERT_FALSE(cfg.has(CollectorConfigField::SamplingIntervalMs));
  cfg.set(CollectorConfigField::SamplingIntervalMs, std::int64_t(500));
  ASSERT_TRUE(cfg.has(CollectorConfigField::SamplingIntervalMs));
  PASS();
}

// ==================== Thread Safety Tests ====================

static void test_thread_safety_concurrent_set() {
  TEST("ThreadSafety: 16 threads setting different fields");
  CollectorConfig cfg;
  std::vector<std::thread> threads;

  for (int t = 0; t < 16; ++t) {
    threads.emplace_back([&cfg, t]() {
      for (int i = 0; i < 1000; ++i) {
        cfg.set(CollectorConfigField::SamplingIntervalMs, std::int64_t(t * 1000 + i));
        cfg.set(CollectorConfigField::MaxEventsPerSecond, std::uint64_t(t * 100 + i));
        cfg.set(CollectorConfigField::Enabled, (i % 2 == 0));
      }
    });
  }

  for (auto& t : threads) t.join();
  ASSERT_TRUE(cfg.isValid());
  PASS();
}

static void test_thread_safety_concurrent_read_write() {
  TEST("ThreadSafety: concurrent reads and writes");
  CollectorConfig cfg;
  std::atomic<bool> stop{false};
  std::atomic<int> readCount{0};

  std::thread writer([&]() {
    for (int i = 0; i < 10000; ++i) {
      cfg.set(CollectorConfigField::SamplingIntervalMs, std::int64_t(i));
    }
    stop = true;
  });

  std::thread reader([&]() {
    while (!stop.load()) {
      cfg.enabled();
      cfg.samplingIntervalMs();
      cfg.mode();
      readCount++;
    }
  });

  writer.join();
  reader.join();
  ASSERT_TRUE(readCount.load() > 0);
  PASS();
}

static void test_thread_safety_concurrent_merge() {
  TEST("ThreadSafety: concurrent merges");
  CollectorConfig cfg;
  std::vector<std::thread> threads;

  for (int t = 0; t < 8; ++t) {
    threads.emplace_back([&cfg, t]() {
      for (int i = 0; i < 500; ++i) {
        CollectorConfig other;
        other.set(CollectorConfigField::SamplingIntervalMs, std::int64_t(t * 1000 + i));
        cfg.merge(other);
      }
    });
  }

  for (auto& t : threads) t.join();
  ASSERT_TRUE(cfg.isValid());
  PASS();
}

// ==================== Default Safety Tests ====================

static void test_safe_defaults_all_positive() {
  TEST("Safety: all numeric defaults are positive");
  CollectorConfig cfg;
  ASSERT_TRUE(cfg.samplingIntervalMs() > 0);
  ASSERT_TRUE(cfg.maxEventsPerSecond() > 0);
  ASSERT_TRUE(cfg.queueLimit() > 0);
  ASSERT_TRUE(cfg.restartBackoffMs() > 0);
  PASS();
}

static void test_safe_defaults_no_dangerous_values() {
  TEST("Safety: no dangerous defaults (0, negative, overflow)");
  CollectorConfig cfg;
  ASSERT_TRUE(cfg.maxRestartAttempts() <= 100);
  ASSERT_TRUE(cfg.restartBackoffMs() <= 300000);
  ASSERT_TRUE(cfg.maxEventsPerSecond() <= 1000000);
  PASS();
}

// ==================== FieldInfo Tests ====================

static void test_field_info_count() {
  TEST("FieldInfo: 9 fields defined");
  ASSERT_EQ(defaultFieldInfos().size(), 9u);
  PASS();
}

static void test_field_info_unique() {
  TEST("FieldInfo: all fields unique");
  auto infos = defaultFieldInfos();
  for (std::size_t i = 0; i < infos.size(); ++i) {
    for (std::size_t j = i + 1; j < infos.size(); ++j) {
      if (infos[i].field == infos[j].field) {
        FAIL("Duplicate field in fieldInfos");
        return;
      }
      if (std::string(infos[i].name) == std::string(infos[j].name)) {
        FAIL("Duplicate name in fieldInfos");
        return;
      }
    }
  }
  PASS();
}

static void test_field_info_has_defaults() {
  TEST("FieldInfo: all fields have defaults");
  for (const auto& info : defaultFieldInfos()) {
    bool hasDefault = false;
    switch (info.type) {
      case ConfigFieldType::Boolean: hasDefault = true; break;
      case ConfigFieldType::Int64:   hasDefault = true; break;
      case ConfigFieldType::UInt64:  hasDefault = true; break;
      case ConfigFieldType::Double:  hasDefault = true; break;
      case ConfigFieldType::String:  hasDefault = true; break;
    }
    if (!hasDefault) {
      FAIL("Field without default");
      return;
    }
  }
  PASS();
}

// ==================== Convenience Accessor Tests ====================

static void test_convenience_vs_raw() {
  TEST("Convenience: typed accessors match raw get");
  CollectorConfig cfg;
  cfg.set(CollectorConfigField::Enabled, false);
  cfg.set(CollectorConfigField::SamplingIntervalMs, std::int64_t(42));
  cfg.set(CollectorConfigField::MaxEventsPerSecond, std::uint64_t(999));
  cfg.set(CollectorConfigField::QueueLimit, std::uint64_t(50000));
  cfg.set(CollectorConfigField::AutoRestart, false);
  cfg.set(CollectorConfigField::MaxRestartAttempts, std::int64_t(7));
  cfg.set(CollectorConfigField::RestartBackoffMs, std::int64_t(10000));
  cfg.set(CollectorConfigField::Mode, std::int64_t(2));
  cfg.set(CollectorConfigField::LogLevel, std::int64_t(5));

  ASSERT_FALSE(cfg.enabled());
  ASSERT_EQ(cfg.samplingIntervalMs(), 42);
  ASSERT_EQ(cfg.maxEventsPerSecond(), 999u);
  ASSERT_EQ(cfg.queueLimit(), 50000u);
  ASSERT_FALSE(cfg.autoRestart());
  ASSERT_EQ(cfg.maxRestartAttempts(), 7u);
  ASSERT_EQ(cfg.restartBackoffMs(), 10000u);
  ASSERT_EQ(cfg.mode(), CollectorMode::Hybrid);
  ASSERT_EQ(cfg.logLevel(), CollectorLogLevel::Trace);
  PASS();
}

// ==================== Performance Tests ====================

static void test_perf_set_get_1m() {
  TEST("Perf: 1M set/get operations");
  CollectorConfig cfg;
  auto start = std::chrono::steady_clock::now();
  for (int i = 0; i < 1000000; ++i) {
    cfg.set(CollectorConfigField::SamplingIntervalMs, std::int64_t(i));
    cfg.samplingIntervalMs();
  }
  auto end = std::chrono::steady_clock::now();
  double ms = std::chrono::duration<double, std::milli>(end - start).count();
  printf("(%.0f ms, %.0fK/s) ", ms, 2000000.0 / ms);
  PASS();
}

static void test_perf_validate_100k() {
  TEST("Perf: 100K validation checks");
  CollectorConfig cfg;
  auto start = std::chrono::steady_clock::now();
  for (int i = 0; i < 100000; ++i) {
    cfg.validate();
  }
  auto end = std::chrono::steady_clock::now();
  double ms = std::chrono::duration<double, std::milli>(end - start).count();
  printf("(%.0f ms, %.0fK/s) ", ms, 100000.0 / ms);
  PASS();
}

// ==================== Main ====================

#ifndef MONIX_KERNEL_BUILD
int main() {
  printf("=== MONIX Collector Configuration Tests ===\n\n");

  printf("[Enum Names]\n");
  test_mode_names();
  test_mode_from_string();
  test_log_level_names();
  test_log_level_from_string();
  test_field_names();
  test_field_type_names();

  printf("\n[Defaults]\n");
  test_defaults_all_present();
  test_defaults_enabled();
  test_defaults_sampling_interval();
  test_defaults_max_events_per_second();
  test_defaults_queue_limit();
  test_defaults_auto_restart();
  test_defaults_max_restart_attempts();
  test_defaults_restart_backoff();
  test_defaults_mode();
  test_defaults_log_level();

  printf("\n[Override]\n");
  test_override_enabled();
  test_override_sampling_interval();
  test_override_max_events();
  test_override_queue_limit();
  test_override_auto_restart();
  test_override_max_restart_attempts();
  test_override_restart_backoff();
  test_override_mode();
  test_override_log_level();

  printf("\n[Validation]\n");
  test_valid_config_passes();
  test_validation_sampling_too_low();
  test_validation_sampling_too_high();
  test_validation_max_events_zero();
  test_validation_queue_limit_below_min();
  test_validation_max_restart_attempts_negative();
  test_validation_restart_backoff_too_low();
  test_validation_boundary_values();

  printf("\n[Merge & Reset]\n");
  test_merge_overrides();
  test_merge_empty();
  test_reset_restores_defaults();

  printf("\n[Field Operations]\n");
  test_remove_field();
  test_has_field();

  printf("\n[Convenience Accessors]\n");
  test_convenience_vs_raw();

  printf("\n[FieldInfo]\n");
  test_field_info_count();
  test_field_info_unique();
  test_field_info_has_defaults();

  printf("\n[Safety]\n");
  test_safe_defaults_all_positive();
  test_safe_defaults_no_dangerous_values();

  printf("\n[Thread Safety]\n");
  test_thread_safety_concurrent_set();
  test_thread_safety_concurrent_read_write();
  test_thread_safety_concurrent_merge();

  printf("\n[Performance]\n");
  test_perf_set_get_1m();
  test_perf_validate_100k();

  printf("\n=== Results: %d passed, %d failed ===\n", gPassed, gFailed);
  return gFailed > 0 ? 1 : 0;
}
#endif

#ifdef MONIX_KERNEL_BUILD
int GetFailedCount_CollectorConfigTests() { return gFailed; }

struct KTestEntry {
  const char* display_name;
  void (*func)();
};

static const KTestEntry s_ktests[] = {
  {"CollectorMode: names", test_mode_names},
  {"CollectorMode: fromString", test_mode_from_string},
  {"CollectorLogLevel: names", test_log_level_names},
  {"CollectorLogLevel: fromString", test_log_level_from_string},
  {"CollectorConfigField: names", test_field_names},
  {"ConfigFieldType: names", test_field_type_names},
  {"Defaults: all present", test_defaults_all_present},
  {"Defaults: enabled", test_defaults_enabled},
  {"Defaults: sampling interval", test_defaults_sampling_interval},
  {"Defaults: max events", test_defaults_max_events_per_second},
  {"Defaults: queue limit", test_defaults_queue_limit},
  {"Defaults: auto restart", test_defaults_auto_restart},
  {"Defaults: max restart attempts", test_defaults_max_restart_attempts},
  {"Defaults: restart backoff", test_defaults_restart_backoff},
  {"Defaults: mode", test_defaults_mode},
  {"Defaults: log level", test_defaults_log_level},
  {"Override: enabled", test_override_enabled},
  {"Override: sampling interval", test_override_sampling_interval},
  {"Override: max events", test_override_max_events},
  {"Override: queue limit", test_override_queue_limit},
  {"Override: auto restart", test_override_auto_restart},
  {"Override: max restart attempts", test_override_max_restart_attempts},
  {"Override: restart backoff", test_override_restart_backoff},
  {"Override: mode", test_override_mode},
  {"Override: log level", test_override_log_level},
  {"Validation: default valid", test_valid_config_passes},
  {"Validation: sampling too low", test_validation_sampling_too_low},
  {"Validation: sampling too high", test_validation_sampling_too_high},
  {"Validation: max events zero", test_validation_max_events_zero},
  {"Validation: queue limit below min", test_validation_queue_limit_below_min},
  {"Validation: max restart negative", test_validation_max_restart_attempts_negative},
  {"Validation: restart backoff low", test_validation_restart_backoff_too_low},
  {"Validation: boundary values", test_validation_boundary_values},
  {"Merge: overrides", test_merge_overrides},
  {"Merge: empty", test_merge_empty},
  {"Reset: restores defaults", test_reset_restores_defaults},
  {"Remove: field", test_remove_field},
  {"Has: field", test_has_field},
  {"Convenience: vs raw", test_convenience_vs_raw},
  {"FieldInfo: count", test_field_info_count},
  {"FieldInfo: unique", test_field_info_unique},
  {"FieldInfo: defaults", test_field_info_has_defaults},
  {"Safety: positive defaults", test_safe_defaults_all_positive},
  {"Safety: no dangerous values", test_safe_defaults_no_dangerous_values},
  {"ThreadSafety: concurrent set", test_thread_safety_concurrent_set},
  {"ThreadSafety: concurrent read write", test_thread_safety_concurrent_read_write},
  {"ThreadSafety: concurrent merge", test_thread_safety_concurrent_merge},
  {"Perf: set get 1m", test_perf_set_get_1m},
  {"Perf: validate 100k", test_perf_validate_100k},
};

const KTestEntry* GetKTests_CollectorConfig() { return s_ktests; }
std::size_t GetKTestCount_CollectorConfig() { return sizeof(s_ktests) / sizeof(s_ktests[0]); }
#endif
