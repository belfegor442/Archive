#include "../../core/collectors/CollectorInfo.hpp"
#include "../../core/collectors/CollectorCapabilities.hpp"
#include "../../core/collectors/CollectorErrors.hpp"
#include "../../core/collectors/CollectorStatus.hpp"
#include "../../core/collectors/CollectorConfig.hpp"
#include "../../core/collectors/CollectorMetrics.hpp"
#include "../../core/collectors/ICollector.hpp"
#include "../../core/collectors/CollectorRegistry.hpp"
#include "../../core/collectors/CollectorManager.hpp"

#include <cassert>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>

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

// ==================== MockCollector ====================

class MockCollector : public ICollector {
public:
  MockCollector(CollectorId id, CollectorCapability caps = CollectorCapability::Snapshot)
    : id_(std::move(id)), caps_(caps) {
    info_.id = id_;
    info_.name = "Mock " + id_;
    info_.version = "1.0.0";
  }

  const CollectorId& id() const override { return id_; }
  const CollectorInfo& info() const override { return info_; }
  CollectorStatus status() const override {
    std::lock_guard<std::mutex> lock(mu_);
    return status_;
  }
  CollectorCapability capabilities() const override { return caps_; }

  bool start(const CollectorConfig&) override {
    std::lock_guard<std::mutex> lock(mu_);
    if (!isValidTransition(status_.lifecycle, CollectorLifecycle::Initializing)) {
      return false;
    }
    status_.lifecycle = CollectorLifecycle::Initializing;
    if (failStart_) {
      status_.lifecycle = CollectorLifecycle::Failed;
      return false;
    }
    status_.lifecycle = CollectorLifecycle::Running;
    startCount_++;
    return true;
  }

  bool stop() override {
    std::lock_guard<std::mutex> lock(mu_);
    if (!isValidTransition(status_.lifecycle, CollectorLifecycle::Stopping)) {
      return false;
    }
    status_.lifecycle = CollectorLifecycle::Stopping;
    status_.lifecycle = CollectorLifecycle::Stopped;
    stopCount_++;
    return true;
  }

  void setEventCallback(EventCallback) override {}

  void simulateDegraded() {
    std::lock_guard<std::mutex> lock(mu_);
    if (status_.lifecycle == CollectorLifecycle::Running) {
      status_.lifecycle = CollectorLifecycle::Degraded;
    }
  }

  void simulateFailStart() { failStart_ = true; }
  void clearFailStart() { failStart_ = false; }

  int startCount() const { return startCount_; }
  int stopCount() const { return stopCount_; }

private:
  CollectorId id_;
  CollectorInfo info_;
  CollectorCapability caps_;
  mutable std::mutex mu_;
  CollectorStatus status_;
  bool failStart_ = false;
  int startCount_ = 0;
  int stopCount_ = 0;
};

// ==================== Type Names Tests ====================

static void test_lifecycle_names() {
  TEST("CollectorLifecycle: all names");
  ASSERT_EQ(std::string(CollectorLifecycleName(CollectorLifecycle::Created)), "Created");
  ASSERT_EQ(std::string(CollectorLifecycleName(CollectorLifecycle::Initializing)), "Initializing");
  ASSERT_EQ(std::string(CollectorLifecycleName(CollectorLifecycle::Running)), "Running");
  ASSERT_EQ(std::string(CollectorLifecycleName(CollectorLifecycle::Degraded)), "Degraded");
  ASSERT_EQ(std::string(CollectorLifecycleName(CollectorLifecycle::Stopping)), "Stopping");
  ASSERT_EQ(std::string(CollectorLifecycleName(CollectorLifecycle::Stopped)), "Stopped");
  ASSERT_EQ(std::string(CollectorLifecycleName(CollectorLifecycle::Failed)), "Failed");
  PASS();
}

static void test_error_code_names() {
  TEST("CollectorErrorCode: all names non-null");
  for (int i = 1; i <= 11; ++i) {
    const char* name = CollectorErrorCodeName(static_cast<CollectorErrorCode>(i));
    ASSERT_TRUE(name != nullptr);
    ASSERT_TRUE(std::string(name) != "Unknown");
  }
  PASS();
}

// ==================== Capability Tests ====================

static void test_capability_bitmask() {
  TEST("CollectorCapability: bitmask operations");
  auto caps = CollectorCapability::Realtime | CollectorCapability::Snapshot;
  ASSERT_TRUE(hasCapability(caps, CollectorCapability::Realtime));
  ASSERT_TRUE(hasCapability(caps, CollectorCapability::Snapshot));
  ASSERT_FALSE(hasCapability(caps, CollectorCapability::Polling));

  caps |= CollectorCapability::Polling;
  ASSERT_TRUE(hasCapability(caps, CollectorCapability::Polling));
  PASS();
}

// ==================== Config Tests ====================

#ifndef MONIX_KERNEL_BUILD
static void test_config_set_get() {
  TEST("CollectorConfig: set and get values");
  CollectorConfig cfg;
  cfg.set("path", std::string("/tmp"));
  cfg.set("interval", std::int64_t(5000));
  cfg.set("enabled", true);

  ASSERT_EQ(cfg.getString("path"), "/tmp");
  ASSERT_EQ(cfg.getInt("interval"), 5000);
  ASSERT_TRUE(cfg.getBool("enabled"));
  ASSERT_TRUE(cfg.has("path"));
  ASSERT_FALSE(cfg.has("missing"));
  PASS();
}

static void test_config_defaults() {
  TEST("CollectorConfig: defaults for missing keys");
  CollectorConfig cfg;
  ASSERT_EQ(cfg.getString("x", "default"), "default");
  ASSERT_EQ(cfg.getInt("x", 42), 42);
  ASSERT_FALSE(cfg.getBool("x", false));
  PASS();
}
#endif

// ==================== Status Tests ====================

static void test_status_can_start() {
  TEST("CollectorStatus: canStart");
  CollectorStatus s;
  s.lifecycle = CollectorLifecycle::Created;
  ASSERT_TRUE(s.canStart());
  s.lifecycle = CollectorLifecycle::Stopped;
  ASSERT_TRUE(s.canStart());
  s.lifecycle = CollectorLifecycle::Failed;
  ASSERT_TRUE(s.canStart());
  s.lifecycle = CollectorLifecycle::Running;
  ASSERT_FALSE(s.canStart());
  PASS();
}

static void test_status_can_stop() {
  TEST("CollectorStatus: canStop");
  CollectorStatus s;
  s.lifecycle = CollectorLifecycle::Running;
  ASSERT_TRUE(s.canStop());
  s.lifecycle = CollectorLifecycle::Degraded;
  ASSERT_TRUE(s.canStop());
  s.lifecycle = CollectorLifecycle::Created;
  ASSERT_FALSE(s.canStop());
  PASS();
}

static void test_status_is_operational() {
  TEST("CollectorStatus: isOperational");
  CollectorStatus s;
  s.lifecycle = CollectorLifecycle::Running;
  ASSERT_TRUE(s.isOperational());
  s.lifecycle = CollectorLifecycle::Degraded;
  ASSERT_TRUE(s.isOperational());
  s.lifecycle = CollectorLifecycle::Stopped;
  ASSERT_FALSE(s.isOperational());
  PASS();
}

// ==================== Lifecycle Transition Tests ====================

static void test_valid_transitions() {
  TEST("Lifecycle: all valid transitions");
  ASSERT_TRUE(isValidTransition(CollectorLifecycle::Created, CollectorLifecycle::Initializing));
  ASSERT_TRUE(isValidTransition(CollectorLifecycle::Initializing, CollectorLifecycle::Running));
  ASSERT_TRUE(isValidTransition(CollectorLifecycle::Initializing, CollectorLifecycle::Failed));
  ASSERT_TRUE(isValidTransition(CollectorLifecycle::Running, CollectorLifecycle::Degraded));
  ASSERT_TRUE(isValidTransition(CollectorLifecycle::Running, CollectorLifecycle::Stopping));
  ASSERT_TRUE(isValidTransition(CollectorLifecycle::Degraded, CollectorLifecycle::Running));
  ASSERT_TRUE(isValidTransition(CollectorLifecycle::Degraded, CollectorLifecycle::Stopping));
  ASSERT_TRUE(isValidTransition(CollectorLifecycle::Stopping, CollectorLifecycle::Stopped));
  ASSERT_TRUE(isValidTransition(CollectorLifecycle::Stopped, CollectorLifecycle::Initializing));
  ASSERT_TRUE(isValidTransition(CollectorLifecycle::Failed, CollectorLifecycle::Initializing));
  PASS();
}

static void test_invalid_transitions() {
  TEST("Lifecycle: invalid transitions rejected");
  ASSERT_FALSE(isValidTransition(CollectorLifecycle::Created, CollectorLifecycle::Running));
  ASSERT_FALSE(isValidTransition(CollectorLifecycle::Created, CollectorLifecycle::Stopped));
  ASSERT_FALSE(isValidTransition(CollectorLifecycle::Created, CollectorLifecycle::Failed));
  ASSERT_FALSE(isValidTransition(CollectorLifecycle::Created, CollectorLifecycle::Degraded));
  ASSERT_FALSE(isValidTransition(CollectorLifecycle::Created, CollectorLifecycle::Stopping));
  ASSERT_FALSE(isValidTransition(CollectorLifecycle::Created, CollectorLifecycle::Created));
  ASSERT_FALSE(isValidTransition(CollectorLifecycle::Running, CollectorLifecycle::Created));
  ASSERT_FALSE(isValidTransition(CollectorLifecycle::Running, CollectorLifecycle::Initializing));
  ASSERT_FALSE(isValidTransition(CollectorLifecycle::Running, CollectorLifecycle::Running));
  ASSERT_FALSE(isValidTransition(CollectorLifecycle::Running, CollectorLifecycle::Stopped));
  ASSERT_FALSE(isValidTransition(CollectorLifecycle::Running, CollectorLifecycle::Failed));
  ASSERT_FALSE(isValidTransition(CollectorLifecycle::Stopping, CollectorLifecycle::Running));
  ASSERT_FALSE(isValidTransition(CollectorLifecycle::Stopping, CollectorLifecycle::Degraded));
  ASSERT_FALSE(isValidTransition(CollectorLifecycle::Stopped, CollectorLifecycle::Running));
  ASSERT_FALSE(isValidTransition(CollectorLifecycle::Stopped, CollectorLifecycle::Created));
  ASSERT_FALSE(isValidTransition(CollectorLifecycle::Failed, CollectorLifecycle::Running));
  ASSERT_FALSE(isValidTransition(CollectorLifecycle::Failed, CollectorLifecycle::Created));
  ASSERT_FALSE(isValidTransition(CollectorLifecycle::Failed, CollectorLifecycle::Stopped));
  PASS();
}

// ==================== Registry Tests ====================

static void test_registry_register() {
  TEST("Registry: register and get");
  CollectorRegistry reg;
  auto c = std::make_shared<MockCollector>("fs");
  ASSERT_EQ(reg.registerCollector(c), CollectorErrorCode::None);
  ASSERT_EQ(reg.size(), 1u);
  ASSERT_TRUE(reg.contains("fs"));
  ASSERT_NE(reg.get("fs"), nullptr);
  PASS();
}

static void test_registry_duplicate() {
  TEST("Registry: duplicate registration rejected");
  CollectorRegistry reg;
  auto c1 = std::make_shared<MockCollector>("fs");
  auto c2 = std::make_shared<MockCollector>("fs");
  ASSERT_EQ(reg.registerCollector(c1), CollectorErrorCode::None);
  ASSERT_EQ(reg.registerCollector(c2), CollectorErrorCode::AlreadyRegistered);
  ASSERT_EQ(reg.size(), 1u);
  PASS();
}

static void test_registry_unregister() {
  TEST("Registry: unregister removes collector");
  CollectorRegistry reg;
  auto c = std::make_shared<MockCollector>("fs");
  reg.registerCollector(c);
  ASSERT_EQ(reg.unregisterCollector("fs"), CollectorErrorCode::None);
  ASSERT_EQ(reg.size(), 0u);
  ASSERT_FALSE(reg.contains("fs"));
  PASS();
}

static void test_registry_unregister_not_found() {
  TEST("Registry: unregister non-existent returns NotRegistered");
  CollectorRegistry reg;
  ASSERT_EQ(reg.unregisterCollector("missing"), CollectorErrorCode::NotRegistered);
  PASS();
}

static void test_registry_list() {
  TEST("Registry: list returns all IDs");
  CollectorRegistry reg;
  reg.registerCollector(std::make_shared<MockCollector>("a"));
  reg.registerCollector(std::make_shared<MockCollector>("b"));
  reg.registerCollector(std::make_shared<MockCollector>("c"));
  auto ids = reg.list();
  ASSERT_EQ(ids.size(), 3u);
  PASS();
}

static void test_registry_clear() {
  TEST("Registry: clear removes all");
  CollectorRegistry reg;
  reg.registerCollector(std::make_shared<MockCollector>("a"));
  reg.registerCollector(std::make_shared<MockCollector>("b"));
  reg.clear();
  ASSERT_EQ(reg.size(), 0u);
  PASS();
}

// ==================== Manager Tests ====================

static void test_manager_start_stop() {
  TEST("Manager: start and stop collector");
  CollectorManager mgr;
  mgr.registerCollector(std::make_shared<MockCollector>("fs"));

  ASSERT_EQ(mgr.startCollector("fs"), CollectorErrorCode::None);
  ASSERT_EQ(mgr.getCollectorStatus("fs").lifecycle, CollectorLifecycle::Running);

  ASSERT_EQ(mgr.stopCollector("fs"), CollectorErrorCode::None);
  ASSERT_EQ(mgr.getCollectorStatus("fs").lifecycle, CollectorLifecycle::Stopped);
  PASS();
}

static void test_manager_start_not_registered() {
  TEST("Manager: start non-existent returns NotRegistered");
  CollectorManager mgr;
  ASSERT_EQ(mgr.startCollector("missing"), CollectorErrorCode::NotRegistered);
  PASS();
}

static void test_manager_start_already_running() {
  TEST("Manager: start running collector returns AlreadyRunning");
  CollectorManager mgr;
  mgr.registerCollector(std::make_shared<MockCollector>("fs"));
  mgr.startCollector("fs");
  ASSERT_EQ(mgr.startCollector("fs"), CollectorErrorCode::AlreadyRunning);
  mgr.stopCollector("fs");
  PASS();
}

static void test_manager_stop_not_running() {
  TEST("Manager: stop non-running returns NotRunning");
  CollectorManager mgr;
  mgr.registerCollector(std::make_shared<MockCollector>("fs"));
  ASSERT_EQ(mgr.stopCollector("fs"), CollectorErrorCode::NotRunning);
  PASS();
}

static void test_manager_start_stop_restart() {
  TEST("Manager: start -> stop -> restart works");
  CollectorManager mgr;
  mgr.registerCollector(std::make_shared<MockCollector>("fs"));

  mgr.startCollector("fs");
  mgr.stopCollector("fs");
  ASSERT_EQ(mgr.startCollector("fs"), CollectorErrorCode::None);
  ASSERT_EQ(mgr.getCollectorStatus("fs").lifecycle, CollectorLifecycle::Running);
  mgr.stopCollector("fs");
  PASS();
}

static void test_manager_start_all_stop_all() {
  TEST("Manager: startAll and stopAll");
  CollectorManager mgr;
  mgr.registerCollector(std::make_shared<MockCollector>("a"));
  mgr.registerCollector(std::make_shared<MockCollector>("b"));
  mgr.registerCollector(std::make_shared<MockCollector>("c"));

  mgr.startAll();
  auto s = mgr.stats();
  ASSERT_EQ(s.running, 3u);

  mgr.stopAll();
  s = mgr.stats();
  ASSERT_EQ(s.stopped, 3u);
  PASS();
}

static void test_manager_stats() {
  TEST("Manager: stats reflect collector states");
  CollectorManager mgr;
  auto mc = std::make_shared<MockCollector>("fs");
  mgr.registerCollector(mc);
  mgr.registerCollector(std::make_shared<MockCollector>("net"));

  mgr.startAll();
  auto s = mgr.stats();
  ASSERT_EQ(s.total_registered, 2u);
  ASSERT_EQ(s.running, 2u);

  mgr.stopAll();
  s = mgr.stats();
  ASSERT_EQ(s.stopped, 2u);
  mgr.stopCollector("fs");
  PASS();
}

static void test_manager_failure_isolation() {
  TEST("Manager: one collector failure doesn't affect others");
  CollectorManager mgr;
  auto good = std::make_shared<MockCollector>("good");
  auto bad = std::make_shared<MockCollector>("bad");
  bad->simulateFailStart();

  mgr.registerCollector(good);
  mgr.registerCollector(bad);

  mgr.startAll();

  ASSERT_EQ(good->status().lifecycle, CollectorLifecycle::Running);
  ASSERT_EQ(bad->status().lifecycle, CollectorLifecycle::Failed);

  auto s = mgr.stats();
  ASSERT_EQ(s.running, 1u);
  ASSERT_EQ(s.failed, 1u);
  mgr.stopAll();
  PASS();
}

static void test_manager_unregister_stops_running() {
  TEST("Manager: unregister stops running collector first");
  CollectorManager mgr;
  auto mc = std::make_shared<MockCollector>("fs");
  mgr.registerCollector(mc);
  mgr.startCollector("fs");
  ASSERT_EQ(mc->status().lifecycle, CollectorLifecycle::Running);

  mgr.unregisterCollector("fs");
  ASSERT_FALSE(mgr.getCollector("fs"));
  PASS();
}

static void test_manager_restart_after_stop() {
  TEST("Manager: restart transitions Stopped -> Initializing");
  CollectorManager mgr;
  auto mc = std::make_shared<MockCollector>("fs");
  mgr.registerCollector(mc);

  mgr.startCollector("fs");
  mgr.stopCollector("fs");
  ASSERT_EQ(mc->status().lifecycle, CollectorLifecycle::Stopped);

  mgr.startCollector("fs");
  ASSERT_EQ(mc->status().lifecycle, CollectorLifecycle::Running);
  mgr.stopCollector("fs");
  PASS();
}

// ==================== Metrics Tests ====================

static void test_metrics_basic() {
  TEST("CollectorMetrics: basic tracking");
  CollectorMetrics m;
  m.recordEventCollected();
  m.recordEventCollected();
  m.recordEventDropped();
  m.recordStart();
  m.recordStop();

  auto s = m.snapshot();
  ASSERT_EQ(s.events_collected, 2u);
  ASSERT_EQ(s.events_dropped, 1u);
  ASSERT_EQ(s.start_count, 1u);
  ASSERT_EQ(s.stop_count, 1u);
  PASS();
}

static void test_metrics_reset() {
  TEST("CollectorMetrics: reset clears all");
  CollectorMetrics m;
  m.recordEventCollected();
  m.recordError();
  m.reset();
  auto s = m.snapshot();
  ASSERT_EQ(s.events_collected, 0u);
  ASSERT_EQ(s.error_count, 0u);
  PASS();
}

// ==================== Concurrency Tests ====================

static void test_concurrent_registry_operations() {
  TEST("Concurrency: 16 threads registering/unregistering");
  CollectorRegistry reg;
  std::atomic<int> successCount{0};
  std::vector<std::thread> threads;

  for (int t = 0; t < 16; ++t) {
    threads.emplace_back([&reg, &successCount, t]() {
      for (int i = 0; i < 100; ++i) {
        CollectorId id = "c" + std::to_string(t * 100 + i);
        auto c = std::make_shared<MockCollector>(std::move(id));
        if (reg.registerCollector(c) == CollectorErrorCode::None) {
          successCount++;
        }
      }
    });
  }

  for (auto& t : threads) t.join();
  ASSERT_EQ(reg.size(), static_cast<std::size_t>(successCount.load()));
  PASS();
}

static void test_concurrent_start_stop() {
  TEST("Concurrency: concurrent start/stop on same collector");
  CollectorManager mgr;
  mgr.registerCollector(std::make_shared<MockCollector>("fs"));

  std::vector<std::thread> threads;
  for (int t = 0; t < 8; ++t) {
    threads.emplace_back([&mgr]() {
      for (int i = 0; i < 50; ++i) {
        mgr.startCollector("fs");
        mgr.stopCollector("fs");
      }
    });
  }

  for (auto& t : threads) t.join();
  auto st = mgr.getCollectorStatus("fs");
  ASSERT_TRUE(st.lifecycle == CollectorLifecycle::Running ||
              st.lifecycle == CollectorLifecycle::Stopped);
  mgr.stopAll();
  PASS();
}

static void test_concurrent_start_all_stop_all() {
  TEST("Concurrency: startAll/stopAll interleaved");
  CollectorManager mgr;
  for (int i = 0; i < 20; ++i) {
    mgr.registerCollector(std::make_shared<MockCollector>("c" + std::to_string(i)));
  }

  std::vector<std::thread> threads;
  for (int t = 0; t < 4; ++t) {
    threads.emplace_back([&mgr]() {
      for (int i = 0; i < 20; ++i) {
        mgr.startAll();
        mgr.stopAll();
      }
    });
  }

  for (auto& t : threads) t.join();
  mgr.stopAll();
  auto s = mgr.stats();
  ASSERT_EQ(s.total_registered, 20u);
  PASS();
}

// ==================== MockCollector Lifecycle Tests ====================

static void test_mock_lifecycle_full() {
  TEST("MockCollector: full lifecycle Created->Running->Stopped");
  MockCollector mc("test");
  ASSERT_EQ(mc.status().lifecycle, CollectorLifecycle::Created);

  CollectorConfig cfg;
  ASSERT_TRUE(mc.start(cfg));
  ASSERT_EQ(mc.status().lifecycle, CollectorLifecycle::Running);

  ASSERT_TRUE(mc.stop());
  ASSERT_EQ(mc.status().lifecycle, CollectorLifecycle::Stopped);
  PASS();
}

static void test_mock_degraded_recovery() {
  TEST("MockCollector: Running -> Degraded -> Running");
  MockCollector mc("test");
  mc.start(CollectorConfig{});
  mc.simulateDegraded();
  ASSERT_EQ(mc.status().lifecycle, CollectorLifecycle::Degraded);

  mc.stop();
  mc.start(CollectorConfig{});
  ASSERT_EQ(mc.status().lifecycle, CollectorLifecycle::Running);
  mc.stop();
  PASS();
}

static void test_mock_restart_from_stopped() {
  TEST("MockCollector: Stopped -> Running restart");
  MockCollector mc("test");
  mc.start(CollectorConfig{});
  mc.stop();
  ASSERT_EQ(mc.status().lifecycle, CollectorLifecycle::Stopped);

  ASSERT_TRUE(mc.start(CollectorConfig{}));
  ASSERT_EQ(mc.status().lifecycle, CollectorLifecycle::Running);
  mc.stop();
  PASS();
}

static void test_mock_restart_from_failed() {
  TEST("MockCollector: Failed -> Running restart");
  MockCollector mc("test");
  mc.simulateFailStart();
  mc.start(CollectorConfig{});
  ASSERT_EQ(mc.status().lifecycle, CollectorLifecycle::Failed);

  mc.clearFailStart();
  ASSERT_TRUE(mc.start(CollectorConfig{}));
  ASSERT_EQ(mc.status().lifecycle, CollectorLifecycle::Running);
  mc.stop();
  PASS();
}

static void test_mock_capabilities() {
  TEST("MockCollector: capabilities preserved");
  MockCollector mc("test", CollectorCapability::Realtime | CollectorCapability::Polling);
  ASSERT_TRUE(hasCapability(mc.capabilities(), CollectorCapability::Realtime));
  ASSERT_TRUE(hasCapability(mc.capabilities(), CollectorCapability::Polling));
  ASSERT_FALSE(hasCapability(mc.capabilities(), CollectorCapability::Snapshot));
  PASS();
}

// ==================== Performance Tests ====================

static void test_perf_registry_100k() {
  TEST("Perf: Registry register 100K collectors");
  CollectorRegistry reg;
  auto start = std::chrono::steady_clock::now();
  for (int i = 0; i < 100000; ++i) {
    reg.registerCollector(std::make_shared<MockCollector>("c" + std::to_string(i)));
  }
  auto end = std::chrono::steady_clock::now();
  double ms = std::chrono::duration<double, std::milli>(end - start).count();
  printf("(%.0f ms, %.0fK/s) ", ms, 100000.0 / ms);
  ASSERT_EQ(reg.size(), 100000u);
  PASS();
}

static void test_perf_manager_lifecycle_10k() {
  TEST("Perf: Manager start/stop 10K collectors");
  CollectorManager mgr;
  for (int i = 0; i < 10000; ++i) {
    mgr.registerCollector(std::make_shared<MockCollector>("c" + std::to_string(i)));
  }

  auto start = std::chrono::steady_clock::now();
  mgr.startAll();
  mgr.stopAll();
  auto end = std::chrono::steady_clock::now();
  double ms = std::chrono::duration<double, std::milli>(end - start).count();
  printf("(%.0f ms, %.0fK/s) ", ms, 10000.0 / ms);
  auto s = mgr.stats();
  ASSERT_EQ(s.stopped, 10000u);
  PASS();
}

// ==================== Main ====================

#ifndef MONIX_KERNEL_BUILD
int main() {
  printf("=== MONIX Collector Foundation Tests ===\n\n");

  printf("[Type Names]\n");
  test_lifecycle_names();
  test_error_code_names();

  printf("\n[Capabilities]\n");
  test_capability_bitmask();

  printf("\n[Config]\n");
#ifndef MONIX_KERNEL_BUILD
  test_config_set_get();
  test_config_defaults();
#endif

  printf("\n[Status]\n");
  test_status_can_start();
  test_status_can_stop();
  test_status_is_operational();

  printf("\n[Lifecycle Transitions]\n");
  test_valid_transitions();
  test_invalid_transitions();

  printf("\n[MockCollector Lifecycle]\n");
  test_mock_lifecycle_full();
  test_mock_degraded_recovery();
  test_mock_restart_from_stopped();
  test_mock_restart_from_failed();
  test_mock_capabilities();

  printf("\n[Registry]\n");
  test_registry_register();
  test_registry_duplicate();
  test_registry_unregister();
  test_registry_unregister_not_found();
  test_registry_list();
  test_registry_clear();

  printf("\n[Manager]\n");
  test_manager_start_stop();
  test_manager_start_not_registered();
  test_manager_start_already_running();
  test_manager_stop_not_running();
  test_manager_start_stop_restart();
  test_manager_start_all_stop_all();
  test_manager_stats();
  test_manager_failure_isolation();
  test_manager_unregister_stops_running();
  test_manager_restart_after_stop();

  printf("\n[Metrics]\n");
  test_metrics_basic();
  test_metrics_reset();

  printf("\n[Concurrency]\n");
  test_concurrent_registry_operations();
  test_concurrent_start_stop();
  test_concurrent_start_all_stop_all();

  printf("\n[Performance]\n");
  test_perf_registry_100k();
  test_perf_manager_lifecycle_10k();

  printf("\n=== Results: %d passed, %d failed ===\n", gPassed, gFailed);
  return gFailed > 0 ? 1 : 0;
}
#endif

#ifdef MONIX_KERNEL_BUILD
int GetFailedCount_CollectorTests() { return gFailed; }

struct KTestEntry {
  const char* display_name;
  void (*func)();
};

static const KTestEntry s_ktests[] = {
  {"CollectorLifecycle: names", test_lifecycle_names},
  {"CollectorErrorCode: names", test_error_code_names},
  {"CollectorCapability: bitmask", test_capability_bitmask},
#ifndef MONIX_KERNEL_BUILD
  {"CollectorConfig: set get", test_config_set_get},
  {"CollectorConfig: defaults", test_config_defaults},
#endif
  {"CollectorStatus: canStart", test_status_can_start},
  {"CollectorStatus: canStop", test_status_can_stop},
  {"CollectorStatus: isOperational", test_status_is_operational},
  {"Lifecycle: valid transitions", test_valid_transitions},
  {"Lifecycle: invalid transitions", test_invalid_transitions},
  {"Registry: register", test_registry_register},
  {"Registry: duplicate", test_registry_duplicate},
  {"Registry: unregister", test_registry_unregister},
  {"Registry: unregister not found", test_registry_unregister_not_found},
  {"Registry: list", test_registry_list},
  {"Registry: clear", test_registry_clear},
  {"Manager: start stop", test_manager_start_stop},
  {"Manager: start not registered", test_manager_start_not_registered},
  {"Manager: start already running", test_manager_start_already_running},
  {"Manager: stop not running", test_manager_stop_not_running},
  {"Manager: start stop restart", test_manager_start_stop_restart},
  {"Manager: startAll stopAll", test_manager_start_all_stop_all},
  {"Manager: stats", test_manager_stats},
  {"Manager: failure isolation", test_manager_failure_isolation},
  {"Manager: unregister stops running", test_manager_unregister_stops_running},
  {"Manager: restart after stop", test_manager_restart_after_stop},
  {"CollectorMetrics: basic", test_metrics_basic},
  {"CollectorMetrics: reset", test_metrics_reset},
  {"Concurrency: registry operations", test_concurrent_registry_operations},
  {"Concurrency: start stop", test_concurrent_start_stop},
  {"Concurrency: startAll stopAll", test_concurrent_start_all_stop_all},
  {"MockCollector: full lifecycle", test_mock_lifecycle_full},
  {"MockCollector: degraded recovery", test_mock_degraded_recovery},
  {"MockCollector: restart from stopped", test_mock_restart_from_stopped},
  {"MockCollector: restart from failed", test_mock_restart_from_failed},
  {"MockCollector: capabilities", test_mock_capabilities},
  {"Perf: registry 100k", test_perf_registry_100k},
  {"Perf: manager lifecycle 10k", test_perf_manager_lifecycle_10k},
};

const KTestEntry* GetKTests_Collector() { return s_ktests; }
std::size_t GetKTestCount_Collector() { return sizeof(s_ktests) / sizeof(s_ktests[0]); }
#endif
