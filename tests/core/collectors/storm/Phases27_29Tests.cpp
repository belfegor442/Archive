#include <cassert>
#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "EventStormProtection.hpp"
#include "QuarantineIntegration.hpp"
#include "ExternalObservation.hpp"

using namespace monix::collectors::storm;
using namespace monix::collectors::quarantine;
using namespace monix::collectors::external;

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

static std::int64_t msNow() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::system_clock::now().time_since_epoch()).count();
}

// ===== Storm Protection Tests =====

static bool test_storm_priority_names() {
  ASSERT_EQ(std::string(StormPriorityName(StormPriority::Critical)), "Critical");
  ASSERT_EQ(std::string(StormPriorityName(StormPriority::Normal)), "Normal");
  ASSERT_EQ(std::string(StormPriorityName(StormPriority::Background)), "Background");
  return true;
}

static bool test_storm_loss_reason_names() {
  ASSERT_EQ(std::string(LossReasonName(LossReason::QueueLimit)), "queue_limit");
  ASSERT_EQ(std::string(LossReasonName(LossReason::RateLimit)), "rate_limit");
  ASSERT_EQ(std::string(LossReasonName(LossReason::Deduplication)), "deduplication");
  return true;
}

static bool test_storm_1k() {
  StormConfig cfg;
  cfg.queue_limit = 500;
  cfg.rate_limit_per_second = 10000;
  cfg.dedup_window_ms = 0;
  cfg.min_priority = StormPriority::Normal;
  StormProtector protector(cfg);

  for (int i = 0; i < 1000; i++) {
    StormEvent e{"E" + std::to_string(i), "fs", "file.modified", StormPriority::Normal, 1000 + i, 0};
    protector.ingest(e);
  }

  ASSERT_TRUE(protector.totalAccepted() >= static_cast<std::size_t>(500));
  ASSERT_TRUE(protector.totalDropped() > static_cast<std::size_t>(0));
  return true;
}

static bool test_storm_10k() {
  StormConfig cfg;
  cfg.queue_limit = 1000;
  cfg.rate_limit_per_second = 100000;
  cfg.dedup_window_ms = 0;
  cfg.min_priority = StormPriority::Normal;
  StormProtector protector(cfg);

  for (int i = 0; i < 10000; i++) {
    StormEvent e{"E" + std::to_string(i), "proc", "process.start", StormPriority::Normal, 1000 + i, 0};
    protector.ingest(e);
  }

  ASSERT_TRUE(protector.totalAccepted() >= static_cast<std::size_t>(1000));
  ASSERT_TRUE(protector.totalDropped() > static_cast<std::size_t>(0));
  return true;
}

static bool test_storm_100k() {
  StormConfig cfg;
  cfg.queue_limit = 5000;
  cfg.rate_limit_per_second = 200000;
  cfg.dedup_window_ms = 0;
  cfg.min_priority = StormPriority::Normal;
  StormProtector protector(cfg);

  for (int i = 0; i < 100000; i++) {
    StormEvent e{"E" + std::to_string(i), "net", "connection", StormPriority::Normal, 1000 + i, 0};
    protector.ingest(e);
  }

  ASSERT_TRUE(protector.totalAccepted() >= static_cast<std::size_t>(5000));
  ASSERT_TRUE(protector.totalDropped() > static_cast<std::size_t>(0));
  ASSERT_EQ(protector.totalIngested(), static_cast<std::size_t>(100000));
  return true;
}

static bool test_storm_1m() {
  StormConfig cfg;
  cfg.queue_limit = 10000;
  cfg.rate_limit_per_second = 2000000;
  cfg.dedup_window_ms = 0;
  cfg.min_priority = StormPriority::Normal;
  StormProtector protector(cfg);

  for (int i = 0; i < 1000000; i++) {
    StormEvent e{"E" + std::to_string(i), "sensor", "reading", StormPriority::Normal, 1000 + i, 0};
    protector.ingest(e);
  }

  ASSERT_TRUE(protector.totalAccepted() >= static_cast<std::size_t>(10000));
  ASSERT_EQ(protector.totalIngested(), static_cast<std::size_t>(1000000));
  return true;
}

static bool test_storm_loss_callback() {
  StormConfig cfg;
  cfg.queue_limit = 10;
  cfg.rate_limit_per_second = 100;
  cfg.dedup_window_ms = 0;
  cfg.min_priority = StormPriority::Normal;
  StormProtector protector(cfg);

  std::size_t loss_count = 0;
  protector.setLossCallback([&](const LossEvent& e) { loss_count++; });

  for (int i = 0; i < 100; i++) {
    StormEvent e{"E" + std::to_string(i), "fs", "mod", StormPriority::Normal, 1000 + i, 0};
    protector.ingest(e);
  }
  protector.flush();

  ASSERT_TRUE(loss_count > static_cast<std::size_t>(0));
  return true;
}

static bool test_storm_priority_filter() {
  StormConfig cfg;
  cfg.queue_limit = 10000;
  cfg.min_priority = StormPriority::High;
  StormProtector protector(cfg);

  protector.ingest({"E1", "fs", "t", StormPriority::Low, 1000, 0});
  protector.ingest({"E2", "fs", "t", StormPriority::High, 1000, 0});
  protector.ingest({"E3", "fs", "t", StormPriority::Critical, 1000, 0});

  ASSERT_EQ(protector.totalAccepted(), static_cast<std::size_t>(2));
  ASSERT_EQ(protector.totalDropped(), static_cast<std::size_t>(1));
  return true;
}

static bool test_storm_dedup() {
  StormConfig cfg;
  cfg.queue_limit = 10000;
  cfg.dedup_window_ms = 1000;
  cfg.min_priority = StormPriority::Normal;
  StormProtector protector(cfg);

  protector.ingest({"E1", "net", "conn", StormPriority::Normal, 1000, 0});
  protector.ingest({"E1", "net", "conn", StormPriority::Normal, 1500, 0});
  protector.ingest({"E2", "net", "conn", StormPriority::Normal, 2000, 0});

  ASSERT_EQ(protector.totalAccepted(), static_cast<std::size_t>(2));
  return true;
}

// ===== Quarantine Integration Tests =====

static bool test_qi_verdict_names() {
  ASSERT_EQ(std::string(ValidationVerdictName(ValidationVerdict::Valid)), "Valid");
  ASSERT_EQ(std::string(ValidationVerdictName(ValidationVerdict::Invalid)), "Invalid");
  ASSERT_EQ(std::string(ValidationVerdictName(ValidationVerdict::Fatal)), "Fatal");
  return true;
}

static bool test_qi_invalid_event() {
  QuarantineIntegration qi;
  CollectorEvent ev{"", "fs", "mod", "fs", 1000};
  auto result = qi.validate(ev);
  ASSERT_TRUE(result.isQuarantined());
  ASSERT_TRUE(qi.quarantine(ev, result));
  ASSERT_EQ(qi.quarantinedCount(), static_cast<std::size_t>(1));
  return true;
}

static bool test_qi_fatal() {
  QuarantineIntegration qi;
  CollectorEvent ev{"E1", "", "t", "s", 1000};
  auto result = qi.validate(ev);
  ASSERT_TRUE(result.isQuarantined());
  ASSERT_TRUE(qi.quarantine(ev, result));
  return true;
}

static bool test_qi_warning() {
  QuarantineIntegration qi;
  CollectorEvent ev{"E1", "fs", "", "s", 1000};
  auto result = qi.validate(ev);
  ASSERT_FALSE(result.isQuarantined());
  ASSERT_EQ(result.verdict, ValidationVerdict::Warning);
  ASSERT_FALSE(qi.quarantine(ev, result));
  return true;
}

static bool test_qi_quarantine_full() {
  QuarantineConfig cfg;
  cfg.max_events = 5;
  QuarantineIntegration qi(cfg);

  for (int i = 0; i < 10; i++) {
    CollectorEvent ev{"E" + std::to_string(i), "", "t", "s", 1000 + i};
    auto result = qi.validate(ev);
    qi.quarantine(ev, result);
  }

  ASSERT_TRUE(qi.quarantinedCount() <= static_cast<std::size_t>(5));
  return true;
}

static bool test_qi_purge() {
  QuarantineConfig cfg;
  cfg.retention_ms = 1000;
  QuarantineIntegration qi(cfg);

  CollectorEvent ev1{"E1", "", "t", "s", 1000};
  auto r1 = qi.validate(ev1);
  qi.quarantine(ev1, r1);

  ASSERT_EQ(qi.quarantinedCount(), static_cast<std::size_t>(1));
  qi.purgeExpired(5000);
  ASSERT_EQ(qi.quarantinedCount(), static_cast<std::size_t>(0));
  return true;
}

static bool test_qi_aggregation() {
  QuarantineIntegration qi;
  for (int i = 0; i < 5; i++) {
    CollectorEvent ev{"", "fs", "", "s", 1000 + i};
    auto result = qi.validate(ev);
    qi.quarantine(ev, result);
  }
  ASSERT_TRUE(qi.aggregateCount() > static_cast<std::size_t>(0));
  auto aggs = qi.aggregates();
  ASSERT_TRUE(aggs.size() > static_cast<std::size_t>(0));
  ASSERT_TRUE(aggs[0].count >= static_cast<std::uint64_t>(1));
  return true;
}

static bool test_qi_drain() {
  QuarantineIntegration qi;
  for (int i = 0; i < 5; i++) {
    CollectorEvent ev{"E" + std::to_string(i), "", "t", "s", 1000 + i};
    auto result = qi.validate(ev);
    qi.quarantine(ev, result);
  }
  auto drained = qi.drain(3);
  ASSERT_EQ(drained.size(), static_cast<std::size_t>(3));
  ASSERT_EQ(qi.quarantinedCount(), static_cast<std::size_t>(2));
  return true;
}

static bool test_qi_callback() {
  QuarantineIntegration qi;
  std::size_t val_calls = 0;
  std::size_t q_calls = 0;
  qi.setValidationCallback([&](const CollectorEvent&, const ValidationResult&) { val_calls++; });
  qi.setQuarantineCallback([&](const QuarantineEntry&) { q_calls++; });

  CollectorEvent ev{"E1", "", "t", "s", 1000};
  auto result = qi.validate(ev);
  qi.quarantine(ev, result);

  ASSERT_TRUE(val_calls >= static_cast<std::size_t>(1));
  ASSERT_TRUE(q_calls >= static_cast<std::size_t>(1));
  return true;
}

// ===== External Observation Tests =====

static bool test_eo_status_names() {
  ASSERT_EQ(std::string(AdapterStatusName(AdapterStatus::Connected)), "Connected");
  ASSERT_EQ(std::string(AdapterStatusName(AdapterStatus::Disconnected)), "Disconnected");
  ASSERT_EQ(std::string(AdapterStatusName(AdapterStatus::Error)), "Error");
  return true;
}

static bool test_eo_kind_names() {
  ASSERT_EQ(std::string(AdapterKindName(AdapterKind::Camera)), "Camera");
  ASSERT_EQ(std::string(AdapterKindName(AdapterKind::NVR)), "NVR");
  ASSERT_EQ(std::string(AdapterKindName(AdapterKind::Router)), "Router");
  ASSERT_EQ(std::string(AdapterKindName(AdapterKind::IoT)), "IoT");
  return true;
}

class MockAdapter : public IExternalObservationAdapter {
public:
  MockAdapter() = default;
  explicit MockAdapter(AdapterKind k) : kind_(k) {}

  const char* name() const override { return "MockAdapter"; }
  const char* version() const override { return "1.0.0"; }
  AdapterKind kind() const override { return kind_; }

  bool connect(const AdapterConfig&) override { connected_ = true; return true; }
  void disconnect() override { connected_ = false; }
  AdapterStatus status() const override { return connected_ ? AdapterStatus::Connected : AdapterStatus::Disconnected; }

  std::vector<NormalizedEvent> receive() override {
    if (!connected_) return {};
    NormalizedEvent ev;
    ev.source_id = "mock-" + std::to_string(id_++);
    ev.event_type = "mock.event";
    ev.description = "mock event";
    ev.timestamp_ms = 1000;
    return {ev};
  }

  NormalizedEvent normalize(const std::string& raw) override {
    NormalizedEvent ev;
    ev.source_id = "normalized";
    ev.event_type = "normalized.event";
    ev.string_value = raw;
    ev.timestamp_ms = 1000;
    return ev;
  }

  void shutdown() override { connected_ = false; }
  AdapterMetrics metrics() const override { return metrics_; }
  bool isConnected() const override { return connected_; }

  AdapterKind kind_ = AdapterKind::Custom;
  bool connected_ = false;
  int id_ = 0;
  AdapterMetrics metrics_;
};

static bool test_eo_register() {
  ExternalObservationManager mgr;
  auto adapter = std::make_unique<MockAdapter>();
  ASSERT_TRUE(mgr.registerAdapter("cam1", std::move(adapter)));
  ASSERT_EQ(mgr.adapterCount(), static_cast<std::size_t>(1));
  return true;
}

static bool test_eo_connect() {
  ExternalObservationManager mgr;
  mgr.registerAdapter("dev1", std::make_unique<MockAdapter>());
  AdapterConfig cfg;
  cfg.id = "dev1";
  cfg.name = "Device 1";
  ASSERT_TRUE(mgr.connect("dev1", cfg));
  ASSERT_TRUE(mgr.isConnected("dev1"));
  return true;
}

static bool test_eo_poll() {
  ExternalObservationManager mgr;
  mgr.registerAdapter("dev1", std::make_unique<MockAdapter>());
  AdapterConfig cfg;
  cfg.id = "dev1";
  cfg.name = "Device 1";
  mgr.connect("dev1", cfg);

  std::size_t event_count = 0;
  mgr.setEventCallback([&](const NormalizedEvent&) { event_count++; });
  auto events = mgr.poll("dev1");
  ASSERT_TRUE(events.size() >= static_cast<std::size_t>(1));
  ASSERT_TRUE(event_count >= static_cast<std::size_t>(1));
  return true;
}

static bool test_eo_disconnect() {
  ExternalObservationManager mgr;
  mgr.registerAdapter("dev1", std::make_unique<MockAdapter>());
  AdapterConfig cfg;
  cfg.id = "dev1";
  cfg.name = "Device 1";
  mgr.connect("dev1", cfg);
  ASSERT_TRUE(mgr.disconnect("dev1"));
  ASSERT_FALSE(mgr.isConnected("dev1"));
  return true;
}

static bool test_eo_shutdown() {
  ExternalObservationManager mgr;
  mgr.registerAdapter("dev1", std::make_unique<MockAdapter>());
  mgr.registerAdapter("dev2", std::make_unique<MockAdapter>());
  mgr.shutdownAll();
  ASSERT_EQ(mgr.status("dev1"), AdapterStatus::Disconnected);
  return true;
}

static bool test_eo_multiple_adapters() {
  ExternalObservationManager mgr;
  mgr.registerAdapter("cam", std::make_unique<MockAdapter>(AdapterKind::Camera));
  mgr.registerAdapter("nvr", std::make_unique<MockAdapter>(AdapterKind::NVR));
  mgr.registerAdapter("router", std::make_unique<MockAdapter>(AdapterKind::Router));

  AdapterConfig cfg;
  cfg.id = "test";
  cfg.name = "Test";
  mgr.connect("cam", cfg);
  mgr.connect("nvr", cfg);

  ASSERT_EQ(mgr.adapterCount(), static_cast<std::size_t>(3));
  ASSERT_EQ(mgr.connectedCount(), static_cast<std::size_t>(2));
  return true;
}

static bool test_eo_normalize() {
  MockAdapter adapter;
  auto ev = adapter.normalize("raw data");
  ASSERT_TRUE(ev.isValid());
  ASSERT_EQ(ev.event_type, "normalized.event");
  return true;
}

static bool test_eo_poll_all() {
  ExternalObservationManager mgr;
  mgr.registerAdapter("a", std::make_unique<MockAdapter>());
  mgr.registerAdapter("b", std::make_unique<MockAdapter>());
  AdapterConfig cfg;
  cfg.id = "test";
  cfg.name = "Test";
  mgr.connect("a", cfg);
  mgr.connect("b", cfg);

  auto events = mgr.pollAll();
  ASSERT_TRUE(events.size() >= static_cast<std::size_t>(2));
  return true;
}

#ifndef MONIX_KERNEL_BUILD
int main() {
  std::cerr << "=== MONIX Phases 27-29 Tests ===\n\n";

  RUN_TEST(storm_priority_names);
  RUN_TEST(storm_loss_reason_names);
  RUN_TEST(storm_1k);
  RUN_TEST(storm_10k);
  RUN_TEST(storm_100k);
  RUN_TEST(storm_1m);
  RUN_TEST(storm_loss_callback);
  RUN_TEST(storm_priority_filter);
  RUN_TEST(storm_dedup);
  RUN_TEST(qi_verdict_names);
  RUN_TEST(qi_invalid_event);
  RUN_TEST(qi_fatal);
  RUN_TEST(qi_warning);
  RUN_TEST(qi_quarantine_full);
  RUN_TEST(qi_purge);
  RUN_TEST(qi_aggregation);
  RUN_TEST(qi_drain);
  RUN_TEST(qi_callback);
  RUN_TEST(eo_status_names);
  RUN_TEST(eo_kind_names);
  RUN_TEST(eo_register);
  RUN_TEST(eo_connect);
  RUN_TEST(eo_poll);
  RUN_TEST(eo_disconnect);
  RUN_TEST(eo_shutdown);
  RUN_TEST(eo_multiple_adapters);
  RUN_TEST(eo_normalize);
  RUN_TEST(eo_poll_all);

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
  {"Storm priority names", test_storm_priority_names},
  {"Storm loss reason names", test_storm_loss_reason_names},
  {"Storm 1k", test_storm_1k},
  {"Storm 10k", test_storm_10k},
  {"Storm 100k", test_storm_100k},
  {"Storm 1m", test_storm_1m},
  {"Storm loss callback", test_storm_loss_callback},
  {"Storm priority filter", test_storm_priority_filter},
  {"Storm dedup", test_storm_dedup},
  {"Quarantine verdict names", test_qi_verdict_names},
  {"Quarantine invalid event", test_qi_invalid_event},
  {"Quarantine fatal", test_qi_fatal},
  {"Quarantine warning", test_qi_warning},
  {"Quarantine full", test_qi_quarantine_full},
  {"Quarantine purge", test_qi_purge},
  {"Quarantine aggregation", test_qi_aggregation},
  {"Quarantine drain", test_qi_drain},
  {"Quarantine callback", test_qi_callback},
  {"External adapter status names", test_eo_status_names},
  {"External adapter kind names", test_eo_kind_names},
  {"External adapter register", test_eo_register},
  {"External adapter connect", test_eo_connect},
  {"External adapter poll", test_eo_poll},
  {"External adapter disconnect", test_eo_disconnect},
  {"External adapter shutdown", test_eo_shutdown},
  {"External adapter multiple", test_eo_multiple_adapters},
  {"External adapter normalize", test_eo_normalize},
  {"External adapter poll all", test_eo_poll_all},
};

const KBoolTestEntry* GetKBoolTests_Phases27_29() { return s_kbooltests; }
std::size_t GetKBoolTestCount_Phases27_29() { return sizeof(s_kbooltests) / sizeof(s_kbooltests[0]); }
#endif
