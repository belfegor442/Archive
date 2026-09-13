#include "../../core/eventbus/EventBusConfig.hpp"
#include "../../core/eventbus/EventBusErrors.hpp"
#include "../../core/eventbus/EventFilter.hpp"
#include "../../core/eventbus/EventSubscription.hpp"
#include "../../core/eventbus/EventProducer.hpp"
#include "../../core/eventbus/EventQueue.hpp"
#include "../../core/eventbus/BackpressureController.hpp"
#include "../../core/eventbus/EventBusMetrics.hpp"
#include "../../core/eventbus/EventBusHealth.hpp"
#include "../../core/eventbus/DeadLetterQueue.hpp"
#include "../../core/eventbus/EventBus.hpp"
#include "../../core/events/EventFactory.hpp"
#include "../../core/events/EventId.hpp"
#include "../../core/events/EventSeverity.hpp"

#include <cassert>
#include <cstdio>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <set>
#include <mutex>
#include <cmath>

using namespace monix::events;
using namespace monix::eventbus;

static int gPassed = 0;
static int gFailed = 0;

#define TEST(name) printf("  %-55s ", name);
#define PASS() do { printf("[PASS]\n"); gPassed++; } while(0)
#define FAIL(msg) do { printf("[FAIL] %s\n", msg); gFailed++; } while(0)
#define ASSERT_TRUE(e) do { if (!(e)) { FAIL(#e); return; } } while(0)
#define ASSERT_FALSE(e) do { if ((e)) { FAIL(#e); return; } } while(0)
#define ASSERT_EQ(a, b) do { if ((a) != (b)) { FAIL(#a " != " #b); return; } } while(0)
#define ASSERT_NE(a, b) do { if ((a) == (b)) { FAIL(#a " == " #b); return; } } while(0)
#define ASSERT_LE(a, b) do { if ((a) > (b)) { FAIL(#a " > " #b); return; } } while(0)
#define ASSERT_GE(a, b) do { if ((a) < (b)) { FAIL(#a " < " #b); return; } } while(0)
#define ASSERT_NEAR(a, b, e) do { if (std::abs((a)-(b)) > (e)) { FAIL(#a " !~= " #b); return; } } while(0)

static SourceRef testSource() {
  return {"test.src", "Test", "1.0", SourceKind::Synthetic};
}

static Event makeEvent(const std::string& ns, const std::string& name, EventSeverity sev = EventSeverity::Info) {
  return EventFactory::createSimple(ns, name, testSource(), sev);
}

// ==================== IngestionResult Tests ====================

static void test_ingestion_result_names() {
  TEST("IngestionResult: all names");
  ASSERT_EQ(std::string(IngestionResultName(IngestionResult::Accepted)), "Accepted");
  ASSERT_EQ(std::string(IngestionResultName(IngestionResult::QueueFull)), "QueueFull");
  ASSERT_EQ(std::string(IngestionResultName(IngestionResult::Invalid)), "Invalid");
  ASSERT_EQ(std::string(IngestionResultName(IngestionResult::Shutdown)), "Shutdown");
  PASS();
}

static void test_consume_result_names() {
  TEST("ConsumeResult: all names");
  ASSERT_EQ(std::string(ConsumeResultName(ConsumeResult::Accepted)), "Accepted");
  ASSERT_EQ(std::string(ConsumeResultName(ConsumeResult::Retry)), "Retry");
  ASSERT_EQ(std::string(ConsumeResultName(ConsumeResult::Failed)), "Failed");
  PASS();
}

static void test_bus_state_names() {
  TEST("EventBusState: all names");
  ASSERT_EQ(std::string(EventBusStateName(EventBusState::Created)), "Created");
  ASSERT_EQ(std::string(EventBusStateName(EventBusState::Running)), "Running");
  ASSERT_EQ(std::string(EventBusStateName(EventBusState::Draining)), "Draining");
  ASSERT_EQ(std::string(EventBusStateName(EventBusState::Stopped)), "Stopped");
  PASS();
}

// ==================== EventFilter Tests ====================

static void test_filter_empty_matches_all() {
  TEST("EventFilter: empty filter matches all");
  EventFilter f;
  ASSERT_TRUE(f.matches(EventType{"fs", "mod"}));
  ASSERT_TRUE(f.matches(SourceId{"any"}));
  ASSERT_TRUE(f.matches(EventSeverity::Info));
  ASSERT_TRUE(f.matches(EventPriority::Normal));
  PASS();
}

static void test_filter_type_match() {
  TEST("EventFilter: type filter matches");
  EventFilter f;
  f.types.push_back({"filesystem", "modified"});
  ASSERT_TRUE(f.matches(EventType{"filesystem", "modified"}));
  ASSERT_FALSE(f.matches(EventType{"process", "started"}));
  ASSERT_TRUE(f.matches(EventType{"filesystem", "modified"}));
  PASS();
}

static void test_filter_namespace_match() {
  TEST("EventFilter: namespace-only filter");
  EventFilter f;
  f.types.push_back({"filesystem", ""});
  ASSERT_TRUE(f.matches(EventType{"filesystem", "created"}));
  ASSERT_TRUE(f.matches(EventType{"filesystem", "modified"}));
  ASSERT_FALSE(f.matches(EventType{"process", "started"}));
  PASS();
}

static void test_filter_severity() {
  TEST("EventFilter: severity filter");
  EventFilter f;
  f.severities.push_back(EventSeverity::Error);
  f.severities.push_back(EventSeverity::Critical);
  ASSERT_TRUE(f.matches(EventSeverity::Error));
  ASSERT_TRUE(f.matches(EventSeverity::Critical));
  ASSERT_FALSE(f.matches(EventSeverity::Info));
  PASS();
}

static void test_default_priority() {
  TEST("defaultPriorityForSeverity: maps correctly");
  ASSERT_EQ(defaultPriorityForSeverity(EventSeverity::Critical), EventPriority::Critical);
  ASSERT_EQ(defaultPriorityForSeverity(EventSeverity::Error), EventPriority::High);
  ASSERT_EQ(defaultPriorityForSeverity(EventSeverity::Warning), EventPriority::Elevated);
  ASSERT_EQ(defaultPriorityForSeverity(EventSeverity::Info), EventPriority::Normal);
  ASSERT_EQ(defaultPriorityForSeverity(EventSeverity::Trace), EventPriority::Background);
  PASS();
}

// ==================== BackpressureController Tests ====================

static void test_backpressure_normal() {
  TEST("BackpressureController: normal at low fill");
  BackpressureController bp(1000);
  bp.update(100);
  ASSERT_EQ(bp.pressure(), QueuePressure::Normal);
  ASSERT_FALSE(bp.shouldDrop(EventPriority::Background));
  ASSERT_FALSE(bp.shouldReject());
  PASS();
}

static void test_backpressure_critical() {
  TEST("BackpressureController: critical at 90%");
  BackpressureController bp(1000);
  bp.update(950);
  ASSERT_EQ(bp.pressure(), QueuePressure::Critical);
  ASSERT_TRUE(bp.shouldDrop(EventPriority::Background));
  ASSERT_TRUE(bp.shouldDrop(EventPriority::Normal));
  ASSERT_FALSE(bp.shouldDrop(EventPriority::High));
  ASSERT_FALSE(bp.shouldReject());
  PASS();
}

static void test_backpressure_full() {
  TEST("BackpressureController: full at 100%");
  BackpressureController bp(1000);
  bp.update(1000);
  ASSERT_EQ(bp.pressure(), QueuePressure::Full);
  ASSERT_TRUE(bp.shouldReject());
  PASS();
}

static void test_backpressure_fill_ratio() {
  TEST("BackpressureController: fill ratio");
  BackpressureController bp(200);
  bp.update(100);
  ASSERT_NEAR(bp.fillRatio(), 0.5, 0.01);
  PASS();
}

// ==================== EventQueue Tests ====================

static void test_queue_push_pop() {
  TEST("EventQueue: basic push/pop");
  EventBusConfig cfg;
  cfg.queue_capacity = 10;
  EventQueue q(cfg);

  auto e = makeEvent("fs", "mod");
  BusEvent be;
  be.event = std::move(e);
  be.priority = EventPriority::Normal;
  ASSERT_TRUE(q.push(std::move(be)));
  ASSERT_EQ(q.size(), 1u);
  ASSERT_FALSE(q.empty());

  auto popped = q.pop();
  ASSERT_TRUE(popped.has_value());
  ASSERT_EQ(q.size(), 0u);
  PASS();
}

static void test_queue_capacity() {
  TEST("EventQueue: respects capacity");
  EventBusConfig cfg;
  cfg.queue_capacity = 5;
  cfg.drop_policy = DropPolicy::RejectNewest;
  EventQueue q(cfg);

  for (int i = 0; i < 5; ++i) {
    BusEvent be;
    be.event = makeEvent("fs", "mod");
    be.priority = EventPriority::Normal;
    ASSERT_TRUE(q.push(std::move(be)));
  }
  ASSERT_TRUE(q.full());

  BusEvent be;
  be.event = makeEvent("fs", "mod");
  be.priority = EventPriority::Normal;
  ASSERT_FALSE(q.pushWithPriority(std::move(be), EventPriority::Normal));
  PASS();
}

static void test_queue_drop_oldest() {
  TEST("EventQueue: DropOldest policy");
  EventBusConfig cfg;
  cfg.queue_capacity = 3;
  cfg.drop_policy = DropPolicy::DropOldest;
  EventQueue q(cfg);

  for (int i = 0; i < 3; ++i) {
    BusEvent be;
    be.event = makeEvent("fs", "mod");
    be.priority = EventPriority::Normal;
    q.pushWithPriority(std::move(be), EventPriority::Normal);
  }

  BusEvent be;
  be.event = makeEvent("fs", "new");
  be.priority = EventPriority::Normal;
  ASSERT_TRUE(q.pushWithPriority(std::move(be), EventPriority::Normal));
  ASSERT_EQ(q.size(), 3u);
  PASS();
}

static void test_queue_close() {
  TEST("EventQueue: close prevents push");
  EventBusConfig cfg;
  cfg.queue_capacity = 10;
  EventQueue q(cfg);
  q.close();

  BusEvent be;
  be.event = makeEvent("fs", "mod");
  be.priority = EventPriority::Normal;
  ASSERT_FALSE(q.push(std::move(be)));
  PASS();
}

static void test_queue_pressure() {
  TEST("EventQueue: pressure updates on push/pop");
  EventBusConfig cfg;
  cfg.queue_capacity = 100;
  EventQueue q(cfg);

  for (int i = 0; i < 50; ++i) {
    BusEvent be;
    be.event = makeEvent("fs", "mod");
    be.priority = EventPriority::Normal;
    q.push(std::move(be));
  }
  ASSERT_EQ(q.pressure(), QueuePressure::Elevated);
  PASS();
}

// ==================== DeadLetterQueue Tests ====================

static void test_deadletter_push_drain() {
  TEST("DeadLetterQueue: push and drain");
  DeadLetterQueue dlq;
  DeadLetterEntry e;
  e.event_id = EventId::generate();
  e.consumer_id = "storage";
  e.failure_reason = "disk_full";
  dlq.push(std::move(e));
  ASSERT_EQ(dlq.size(), 1u);

  auto entries = dlq.drain();
  ASSERT_EQ(entries.size(), 1u);
  ASSERT_EQ(dlq.size(), 0u);
  PASS();
}

static void test_deadletter_aggregates() {
  TEST("DeadLetterQueue: multiple pushes");
  DeadLetterQueue dlq;
  for (int i = 0; i < 10; ++i) {
    DeadLetterEntry e;
    e.event_id = EventId::generate();
    e.consumer_id = "storage";
    dlq.push(std::move(e));
  }
  ASSERT_EQ(dlq.size(), 10u);
  dlq.clear();
  ASSERT_EQ(dlq.size(), 0u);
  PASS();
}

// ==================== EventBusMetrics Tests ====================

static void test_metrics_basic() {
  TEST("EventBusMetrics: received/accepted/dropped");
  EventBusMetrics m;
  m.recordReceived("fs");
  m.recordReceived("fs");
  m.recordAccepted("fs");
  m.recordDropped("fs", "queue_overflow");

  auto s = m.snapshot();
  ASSERT_EQ(s.events_received, 2u);
  ASSERT_EQ(s.events_accepted, 1u);
  ASSERT_EQ(s.events_dropped, 1u);

  auto src = m.sourceMetrics();
  ASSERT_EQ(src["fs"].received, 2u);
  ASSERT_EQ(src["fs"].dropped, 1u);
  PASS();
}

static void test_metrics_loss_accounting() {
  TEST("EventBusMetrics: loss accounting aggregated");
  EventBusMetrics m;
  m.recordDropped("fs", "overflow");
  m.recordDropped("fs", "overflow");
  m.recordDropped("net", "overflow");

  auto losses = m.lossRecords();
  ASSERT_EQ(losses.size(), 2u);
  for (const auto& lr : losses) {
    if (lr.source == "fs") ASSERT_EQ(lr.count, 2u);
    if (lr.source == "net") ASSERT_EQ(lr.count, 1u);
  }
  PASS();
}

// ==================== EventSubscription Tests ====================

static void test_subscription_cancel() {
  TEST("EventSubscription: cancel invalidates");
  SubscriptionId nextId = 1;
  bool cancelled = false;
  EventSubscription sub(nextId++, [&]() { cancelled = true; });
  ASSERT_TRUE(sub.isValid());
  sub.cancel();
  ASSERT_FALSE(sub.isValid());
  ASSERT_TRUE(cancelled);
  PASS();
}

// ==================== EventBus Publish Tests ====================

static void test_bus_publish_accepted() {
  TEST("EventBus: publish accepted");
  EventBusConfig cfg;
  cfg.queue_capacity = 100;
  cfg.consumer_worker_count = 1;
  EventBus bus(cfg);
  bus.start();

  auto result = bus.publish(makeEvent("fs", "mod"));
  ASSERT_EQ(result, IngestionResult::Accepted);

  auto h = bus.health();
  ASSERT_EQ(h.received, 1u);
  ASSERT_EQ(h.accepted, 1u);
  bus.stop();
  PASS();
}

static void test_bus_publish_invalid() {
  TEST("EventBus: publish invalid event");
  EventBusConfig cfg;
  cfg.consumer_worker_count = 1;
  EventBus bus(cfg);
  bus.start();

  Event bad;
  bad.id = EventId{};
  bad.type = {"fs", "mod"};
  auto result = bus.publish(std::move(bad));
  ASSERT_EQ(result, IngestionResult::Invalid);
  bus.stop();
  PASS();
}

static void test_bus_publish_shutdown() {
  TEST("EventBus: publish after stop returns Shutdown");
  EventBusConfig cfg;
  cfg.consumer_worker_count = 1;
  EventBus bus(cfg);
  bus.start();
  bus.stop();

  auto result = bus.publish(makeEvent("fs", "mod"));
  ASSERT_EQ(result, IngestionResult::Shutdown);
  PASS();
}

static void test_bus_queue_full() {
  TEST("EventBus: queue full returns QueueFull");
  EventBusConfig cfg;
  cfg.queue_capacity = 2;
  cfg.consumer_worker_count = 0;
  cfg.drop_policy = DropPolicy::RejectNewest;
  EventBus bus(cfg);
  bus.start();

  ASSERT_EQ(bus.publish(makeEvent("fs", "m1")), IngestionResult::Accepted);
  ASSERT_EQ(bus.publish(makeEvent("fs", "m2")), IngestionResult::Accepted);
  ASSERT_EQ(bus.publish(makeEvent("fs", "m3")), IngestionResult::QueueFull);
  bus.stop();
  PASS();
}

// ==================== Consumer Delivery Tests ====================

class CollectingConsumer : public IEventConsumer {
public:
  ConsumeResult consume(const Event& event) override {
    std::lock_guard<std::mutex> lock(mu);
    ids.push_back(event.id);
    count++;
    return ConsumeResult::Accepted;
  }
  std::vector<EventId> ids;
  std::atomic<int> count{0};
  std::mutex mu;
};

class FailingConsumer : public IEventConsumer {
public:
  explicit FailingConsumer(int failCount) : failCount_(failCount) {}
  ConsumeResult consume(const Event&) override {
    int attempt = attempt_++;
    if (attempt < failCount_) return ConsumeResult::Retry;
    return ConsumeResult::Accepted;
  }
  int attempt_ = 0;
  int failCount_;
};

class AlwaysFailConsumer : public IEventConsumer {
public:
  ConsumeResult consume(const Event&) override {
    count++;
    return ConsumeResult::Failed;
  }
  std::atomic<int> count{0};
};

static void test_bus_consumer_receives() {
  TEST("EventBus: subscriber receives events");
  EventBusConfig cfg;
  cfg.consumer_worker_count = 2;
  EventBus bus(cfg);

  auto collector = std::make_shared<CollectingConsumer>();
  EventFilter filter;
  bus.subscribe(filter, collector);
  bus.start();

  for (int i = 0; i < 100; ++i) {
    bus.publish(makeEvent("fs", "mod"));
  }

  auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (collector->count < 100 && std::chrono::steady_clock::now() < deadline) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  ASSERT_EQ(collector->count.load(), 100);
  bus.stop();
  PASS();
}

static void test_bus_filter_delivers_correctly() {
  TEST("EventBus: filter only delivers matching events");
  EventBusConfig cfg;
  cfg.consumer_worker_count = 2;
  EventBus bus(cfg);

  auto collector = std::make_shared<CollectingConsumer>();
  EventFilter filter;
  filter.types.push_back({"filesystem", ""});
  bus.subscribe(filter, collector);
  bus.start();

  bus.publish(makeEvent("filesystem", "modified"));
  bus.publish(makeEvent("process", "started"));
  bus.publish(makeEvent("filesystem", "created"));
  bus.publish(makeEvent("network", "connected"));

  auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (collector->count < 2 && std::chrono::steady_clock::now() < deadline) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  ASSERT_EQ(collector->count.load(), 2);
  bus.stop();
  PASS();
}

static void test_bus_multiple_subscribers() {
  TEST("EventBus: event reaches all matching subscribers");
  EventBusConfig cfg;
  cfg.consumer_worker_count = 2;
  EventBus bus(cfg);

  auto c1 = std::make_shared<CollectingConsumer>();
  auto c2 = std::make_shared<CollectingConsumer>();
  EventFilter filter;
  bus.subscribe(filter, c1);
  bus.subscribe(filter, c2);
  bus.start();

  bus.publish(makeEvent("fs", "mod"));

  auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (c1->count < 1 && std::chrono::steady_clock::now() < deadline) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  while (c2->count < 1 && std::chrono::steady_clock::now() < deadline) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  ASSERT_EQ(c1->count.load(), 1);
  ASSERT_EQ(c2->count.load(), 1);
  bus.stop();
  PASS();
}

static void test_bus_consumer_failure_goes_to_dlq() {
  TEST("EventBus: failed consumer goes to dead letter queue");
  EventBusConfig cfg;
  cfg.consumer_worker_count = 2;
  cfg.max_retries = 0;
  EventBus bus(cfg);

  auto failer = std::make_shared<AlwaysFailConsumer>();
  EventFilter filter;
  bus.subscribe(filter, failer);
  bus.start();

  bus.publish(makeEvent("fs", "mod"));

  auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (failer->count < 1 && std::chrono::steady_clock::now() < deadline) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  ASSERT_EQ(failer->count.load(), 1);
  ASSERT_EQ(bus.deadLetters().size(), 1u);
  bus.stop();
  PASS();
}

static void test_bus_retry_succeeds() {
  TEST("EventBus: retry succeeds after failures");
  EventBusConfig cfg;
  cfg.consumer_worker_count = 2;
  cfg.max_retries = 3;
  cfg.retry_delay = std::chrono::milliseconds(10);
  EventBus bus(cfg);

  auto flaky = std::make_shared<FailingConsumer>(2);
  EventFilter filter;
  bus.subscribe(filter, flaky);
  bus.start();

  bus.publish(makeEvent("fs", "mod"));

  auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (flaky->attempt_ < 3 && std::chrono::steady_clock::now() < deadline) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  ASSERT_GE(flaky->attempt_, 3);
  ASSERT_EQ(bus.deadLetters().size(), 0u);
  bus.stop();
  PASS();
}

// ==================== Concurrency Tests ====================

static void test_concurrent_publish() {
  TEST("Concurrency: 16 producers x 10K events");
  EventBusConfig cfg;
  cfg.queue_capacity = 200000;
  cfg.consumer_worker_count = 4;
  EventBus bus(cfg);

  auto collector = std::make_shared<CollectingConsumer>();
  EventFilter filter;
  bus.subscribe(filter, collector);
  bus.start();

  std::atomic<int> acceptedCount{0};
  std::vector<std::thread> producers;

  for (int t = 0; t < 16; ++t) {
    producers.emplace_back([&]() {
      for (int i = 0; i < 10000; ++i) {
        auto result = bus.publish(makeEvent("fs", "mod"));
        if (result == IngestionResult::Accepted) acceptedCount++;
      }
    });
  }

  for (auto& t : producers) t.join();

  auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
  while (collector->count < acceptedCount && std::chrono::steady_clock::now() < deadline) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  ASSERT_EQ(collector->count.load(), acceptedCount.load());
  auto h = bus.health();
  ASSERT_EQ(h.received, static_cast<std::uint64_t>(acceptedCount.load()));
  bus.stop();
  PASS();
}

// ==================== Shutdown Tests ====================

static void test_shutdown_drains() {
  TEST("Shutdown: drains pending events");
  EventBusConfig cfg;
  cfg.queue_capacity = 10000;
  cfg.consumer_worker_count = 0;
  EventBus bus(cfg);
  bus.start();

  for (int i = 0; i < 1000; ++i) {
    bus.publish(makeEvent("fs", "mod"));
  }

  auto hBefore = bus.health();
  ASSERT_EQ(hBefore.received, 1000u);

  bus.shutdown();
  ASSERT_EQ(bus.state(), EventBusState::Stopped);
  PASS();
}

static void test_restart_clean() {
  TEST("Restart: start/stop/start does not corrupt state");
  EventBusConfig cfg;
  cfg.consumer_worker_count = 1;
  EventBus bus(cfg);

  bus.start();
  bus.publish(makeEvent("fs", "mod"));
  bus.stop();
  bus.start();

  auto result = bus.publish(makeEvent("fs", "mod2"));
  ASSERT_EQ(result, IngestionResult::Accepted);
  bus.stop();
  PASS();
}

// ==================== Health Tests ====================

static void test_health_snapshot() {
  TEST("EventBusHealth: snapshot reflects state");
  EventBusConfig cfg;
  cfg.queue_capacity = 500;
  cfg.consumer_worker_count = 1;
  EventBus bus(cfg);
  bus.start();

  bus.publish(makeEvent("fs", "mod"));
  auto h = bus.health();
  ASSERT_EQ(h.state, EventBusState::Running);
  ASSERT_EQ(h.received, 1u);
  ASSERT_EQ(h.queue_capacity, 500u);
  bus.stop();
  PASS();
}

// ==================== Performance Baseline ====================

static void test_perf_ingestion() {
  TEST("Perf: 500K publish throughput");
  EventBusConfig cfg;
  cfg.queue_capacity = 200000;
  cfg.consumer_worker_count = 4;
  cfg.drop_policy = DropPolicy::RejectNewest;
  EventBus bus(cfg);

  auto nullConsumer = std::make_shared<CollectingConsumer>();
  EventFilter filter;
  bus.subscribe(filter, nullConsumer);
  bus.start();

  auto start = std::chrono::steady_clock::now();
  std::uint64_t accepted = 0;
  for (int i = 0; i < 500000; ++i) {
    if (bus.publish(makeEvent("fs", "mod")) == IngestionResult::Accepted) accepted++;
  }
  auto end = std::chrono::steady_clock::now();
  double ms = std::chrono::duration<double, std::milli>(end - start).count();
  printf("(%.0f ms, %.0fK events/s) ", ms, accepted / ms);

  auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
  while (nullConsumer->count < static_cast<int>(accepted) && std::chrono::steady_clock::now() < deadline) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  bus.stop();
  PASS();
}

static void test_perf_backpressure() {
  TEST("Backpressure: pressure levels update correctly");
  BackpressureController bp(1000);
  bp.update(100);
  ASSERT_EQ(bp.pressure(), QueuePressure::Normal);
  bp.update(600);
  ASSERT_EQ(bp.pressure(), QueuePressure::Elevated);
  bp.update(750);
  ASSERT_EQ(bp.pressure(), QueuePressure::High);
  bp.update(950);
  ASSERT_EQ(bp.pressure(), QueuePressure::Critical);
  bp.update(1000);
  ASSERT_EQ(bp.pressure(), QueuePressure::Full);
  PASS();
}

// ==================== Main ====================

#ifndef MONIX_KERNEL_BUILD
int main() {
  printf("=== MONIX Event Bus Tests ===\n\n");

  printf("[Error Model]\n");
  test_ingestion_result_names();
  test_consume_result_names();
  test_bus_state_names();

  printf("\n[EventFilter]\n");
  test_filter_empty_matches_all();
  test_filter_type_match();
  test_filter_namespace_match();
  test_filter_severity();
  test_default_priority();

  printf("\n[Backpressure]\n");
  test_backpressure_normal();
  test_backpressure_critical();
  test_backpressure_full();
  test_backpressure_fill_ratio();
  test_perf_backpressure();

  printf("\n[EventQueue]\n");
  test_queue_push_pop();
  test_queue_capacity();
  test_queue_drop_oldest();
  test_queue_close();
  test_queue_pressure();

  printf("\n[DeadLetterQueue]\n");
  test_deadletter_push_drain();
  test_deadletter_aggregates();

  printf("\n[Metrics]\n");
  test_metrics_basic();
  test_metrics_loss_accounting();

  printf("\n[Subscription]\n");
  test_subscription_cancel();

  printf("\n[EventBus Publish]\n");
  test_bus_publish_accepted();
  test_bus_publish_invalid();
  test_bus_publish_shutdown();
  test_bus_queue_full();

  printf("\n[Consumer Delivery]\n");
  test_bus_consumer_receives();
  test_bus_filter_delivers_correctly();
  test_bus_multiple_subscribers();
  test_bus_consumer_failure_goes_to_dlq();
  test_bus_retry_succeeds();

  printf("\n[Shutdown]\n");
  test_shutdown_drains();
  test_restart_clean();

  printf("\n[Health]\n");
  test_health_snapshot();

  printf("\n[Concurrency]\n");
  test_concurrent_publish();

  printf("\n[Performance]\n");
  test_perf_ingestion();

  printf("\n=== Results: %d passed, %d failed ===\n", gPassed, gFailed);
  return gFailed > 0 ? 1 : 0;
}
#endif

#ifdef MONIX_KERNEL_BUILD
int GetFailedCount_EventBusTests() { return gFailed; }

struct KTestEntry {
  const char* display_name;
  void (*func)();
};

static const KTestEntry s_ktests[] = {
  {"IngestionResult: names", test_ingestion_result_names},
  {"ConsumeResult: names", test_consume_result_names},
  {"EventBusState: names", test_bus_state_names},
  {"EventFilter: empty matches all", test_filter_empty_matches_all},
  {"EventFilter: type match", test_filter_type_match},
  {"EventFilter: namespace match", test_filter_namespace_match},
  {"EventFilter: severity", test_filter_severity},
  {"Default priority", test_default_priority},
  {"BackpressureController: normal", test_backpressure_normal},
  {"BackpressureController: critical", test_backpressure_critical},
  {"BackpressureController: full", test_backpressure_full},
  {"BackpressureController: fill ratio", test_backpressure_fill_ratio},
  {"EventQueue: push pop", test_queue_push_pop},
  {"EventQueue: capacity", test_queue_capacity},
  {"EventQueue: drop oldest", test_queue_drop_oldest},
  {"EventQueue: close", test_queue_close},
  {"EventQueue: pressure", test_queue_pressure},
  {"DeadLetterQueue: push drain", test_deadletter_push_drain},
  {"DeadLetterQueue: aggregates", test_deadletter_aggregates},
  {"EventBusMetrics: basic", test_metrics_basic},
  {"EventBusMetrics: loss accounting", test_metrics_loss_accounting},
  {"EventSubscription: cancel", test_subscription_cancel},
  {"EventBus: publish accepted", test_bus_publish_accepted},
  {"EventBus: publish invalid", test_bus_publish_invalid},
  {"EventBus: publish shutdown", test_bus_publish_shutdown},
  {"EventBus: queue full", test_bus_queue_full},
  {"EventBus: consumer receives", test_bus_consumer_receives},
  {"EventBus: filter delivers", test_bus_filter_delivers_correctly},
  {"EventBus: multiple subscribers", test_bus_multiple_subscribers},
  {"EventBus: consumer failure to dlq", test_bus_consumer_failure_goes_to_dlq},
  {"EventBus: retry succeeds", test_bus_retry_succeeds},
  {"Concurrency: 16 producers", test_concurrent_publish},
  {"Shutdown: drains", test_shutdown_drains},
  {"Restart: clean", test_restart_clean},
  {"EventBusHealth: snapshot", test_health_snapshot},
  {"Perf: 500K publish", test_perf_ingestion},
  {"Backpressure: levels", test_perf_backpressure},
};

const KTestEntry* GetKTests_EventBus() { return s_ktests; }
std::size_t GetKTestCount_EventBus() { return sizeof(s_ktests) / sizeof(s_ktests[0]); }
#endif
