#include "../../core/events/EventId.hpp"
#include "../../core/events/EventTime.hpp"
#include "../../core/events/EventType.hpp"
#include "../../core/events/EventSeverity.hpp"
#include "../../core/events/EventFlags.hpp"
#include "../../core/events/SourceRef.hpp"
#include "../../core/events/ActorRef.hpp"
#include "../../core/events/EntityRef.hpp"
#include "../../core/events/Action.hpp"
#include "../../core/events/Correlation.hpp"
#include "../../core/events/Provenance.hpp"
#include "../../core/events/EventPayload.hpp"
#include "../../core/events/Event.hpp"
#include "../../core/events/EventBuilder.hpp"
#include "../../core/events/EventFactory.hpp"
#include "../../core/events/EventSerializer.hpp"
#include "../../core/events/EventDeserializer.hpp"
#include "../../core/events/EventErrors.hpp"

#include <cassert>
#include <cstdio>
#include <cmath>
#include <string>
#include <set>
#include <thread>
#include <vector>
#include <chrono>
#include <mutex>

using namespace monix::events;

static int gPassed = 0;
static int gFailed = 0;

#define TEST(name) printf("  %-50s ", name);
#define PASS() do { printf("[PASS]\n"); gPassed++; } while(0)
#define FAIL(msg) do { printf("[FAIL] %s\n", msg); gFailed++; } while(0)
#define ASSERT_TRUE(e) do { if (!(e)) { FAIL(#e); return; } } while(0)
#define ASSERT_FALSE(e) do { if ((e)) { FAIL(#e); return; } } while(0)
#define ASSERT_EQ(a, b) do { if ((a) != (b)) { FAIL(#a " != " #b); return; } } while(0)
#define ASSERT_NE(a, b) do { if ((a) == (b)) { FAIL(#a " == " #b); return; } } while(0)
#define ASSERT_NEAR(a, b, e) do { if (std::abs((a)-(b)) > (e)) { FAIL(#a " !~= " #b); return; } } while(0)

// ==================== EventId Tests ====================

static void test_eventid_generate_unique() {
  TEST("EventId: 1M unique IDs");
  std::set<std::string> ids;
  for (int i = 0; i < 1000000; ++i) {
    auto id = EventId::generate();
    auto s = id.toString();
    if (ids.count(s)) { FAIL("duplicate ID found"); return; }
    ids.insert(s);
  }
  ASSERT_EQ(ids.size(), 1000000u);
  PASS();
}

static void test_eventid_serialization_roundtrip() {
  TEST("EventId: serialize/deserialize roundtrip");
  for (int i = 0; i < 10000; ++i) {
    auto id = EventId::generate();
    std::string str = id.toString();
    auto id2 = EventId::fromString(str);
    ASSERT_TRUE(id == id2);
  }
  PASS();
}

static void test_eventid_bytes_roundtrip() {
  TEST("EventId: bytes roundtrip");
  auto id = EventId::generate();
  auto bytes = id.toBytes();
  auto id2 = EventId::fromBytes(bytes);
  ASSERT_TRUE(id == id2);
  PASS();
}

static void test_eventid_invalid_string() {
  TEST("EventId: invalid string returns invalid");
  auto id = EventId::fromString("not-a-valid-id");
  ASSERT_FALSE(id.isValid());
  PASS();
}

static void test_eventid_empty() {
  TEST("EventId: default is invalid");
  EventId id;
  ASSERT_FALSE(id.isValid());
  PASS();
}

static void test_eventid_format() {
  TEST("EventId: format is 8-4-4-4-12");
  auto id = EventId::generate();
  auto s = id.toString();
  ASSERT_EQ(s.size(), 36u);
  ASSERT_EQ(s[8], '-');
  ASSERT_EQ(s[13], '-');
  ASSERT_EQ(s[18], '-');
  ASSERT_EQ(s[23], '-');
  PASS();
}

// ==================== EventType Tests ====================

static void test_eventtype_valid() {
  TEST("EventType: valid type");
  EventType t{"filesystem", "modified", 1203};
  ASSERT_TRUE(t.isValid());
  ASSERT_EQ(t.qualifiedName(), "filesystem.modified");
  PASS();
}

static void test_eventtype_invalid_empty() {
  TEST("EventType: empty namespace invalid");
  EventType t{"", "modified"};
  ASSERT_FALSE(t.isValid());
  PASS();
}

// ==================== EventSeverity Tests ====================

static void test_severity_names() {
  TEST("EventSeverity: all names");
  ASSERT_EQ(std::string(EventSeverityName(EventSeverity::Trace)), "trace");
  ASSERT_EQ(std::string(EventSeverityName(EventSeverity::Debug)), "debug");
  ASSERT_EQ(std::string(EventSeverityName(EventSeverity::Info)), "info");
  ASSERT_EQ(std::string(EventSeverityName(EventSeverity::Notice)), "notice");
  ASSERT_EQ(std::string(EventSeverityName(EventSeverity::Warning)), "warning");
  ASSERT_EQ(std::string(EventSeverityName(EventSeverity::Error)), "error");
  ASSERT_EQ(std::string(EventSeverityName(EventSeverity::Critical)), "critical");
  PASS();
}

static void test_severity_from_string() {
  TEST("EventSeverity: from string");
  ASSERT_EQ(EventSeverityFromString("info"), EventSeverity::Info);
  ASSERT_EQ(EventSeverityFromString("warning"), EventSeverity::Warning);
  ASSERT_EQ(EventSeverityFromString("unknown"), EventSeverity::Info);
  PASS();
}

// ==================== EventFlags Tests ====================

static void test_flags_operators() {
  TEST("EventFlags: bitwise operators");
  EventFlags f = EventFlags::None;
  f |= EventFlags::Derived;
  f |= EventFlags::Late;
  ASSERT_TRUE(hasFlag(f, EventFlags::Derived));
  ASSERT_TRUE(hasFlag(f, EventFlags::Late));
  ASSERT_FALSE(hasFlag(f, EventFlags::Synthetic));
  PASS();
}

// ==================== SourceRef Tests ====================

static void test_sourceref_valid() {
  TEST("SourceRef: valid source");
  SourceRef s{"windows.filesystem", "FS Collector", "1.0", SourceKind::OperatingSystem};
  ASSERT_TRUE(s.isValid());
  PASS();
}

static void test_sourceref_invalid() {
  TEST("SourceRef: empty id invalid");
  SourceRef s{"", "name", "1.0"};
  ASSERT_FALSE(s.isValid());
  PASS();
}

// ==================== ActorRef Tests ====================

static void test_actorref_optional_fields() {
  TEST("ActorRef: optional fields");
  ActorRef a;
  a.id = "process:4218";
  a.kind = ActorKind::Process;
  a.process_id = 4218;
  ASSERT_TRUE(a.isValid());
  ASSERT_EQ(a.process_id.value(), 4218u);
  ASSERT_FALSE(a.username.has_value());
  PASS();
}

// ==================== EntityRef Tests ====================

static void test_entityref_optional() {
  TEST("EntityRef: optional name");
  EntityRef e;
  e.id = "file:12345";
  e.kind = EntityKind::File;
  ASSERT_TRUE(e.isValid());
  ASSERT_FALSE(e.name.has_value());
  e.name = "config.json";
  ASSERT_TRUE(e.name.has_value());
  PASS();
}

// ==================== Action Tests ====================

static void test_action_valid() {
  TEST("Action: valid action");
  Action a{"modify"};
  ASSERT_TRUE(a.isValid());
  PASS();
}

static void test_action_with_id() {
  TEST("Action: with numeric id");
  Action a{"write", 42};
  ASSERT_TRUE(a.isValid());
  ASSERT_EQ(a.numeric_id.value(), 42u);
  PASS();
}

// ==================== Correlation Tests ====================

static void test_correlation_hasany() {
  TEST("Correlation: hasAny");
  Correlation c;
  ASSERT_FALSE(c.hasAny());
  c.activity_id = "act:123";
  ASSERT_TRUE(c.hasAny());
  PASS();
}

// ==================== Provenance Tests ====================

static void test_provenance_valid() {
  TEST("Provenance: valid provenance");
  Provenance p;
  p.source_id = "windows.fs";
  p.collector_name = "FS Collector";
  p.kind = ProvenanceKind::NativeObservation;
  ASSERT_TRUE(p.isValid());
  PASS();
}

static void test_provenance_kind_names() {
  TEST("ProvenanceKind: all names");
  ASSERT_EQ(std::string(ProvenanceKindName(ProvenanceKind::NativeObservation)), "native_observation");
  ASSERT_EQ(std::string(ProvenanceKindName(ProvenanceKind::Derived)), "derived");
  PASS();
}

// ==================== EventPayload Tests ====================

static void test_payload_set_get() {
  TEST("EventPayload: set and get typed values");
  EventPayload p;
  p.set("path", std::string("C:\\test.txt"));
  p.set("size", static_cast<std::int64_t>(18321));
  p.set("ratio", 3.14);
  p.set("readable", true);

  ASSERT_EQ(p.getString("path"), "C:\\test.txt");
  ASSERT_EQ(p.getInt("size"), 18321);
  ASSERT_NEAR(p.getDouble("ratio"), 3.14, 0.001);
  ASSERT_TRUE(p.getBool("readable"));
  ASSERT_FALSE(p.getBool("missing"));
  ASSERT_EQ(p.getString("missing", "default"), "default");
  PASS();
}

static void test_payload_overwrite() {
  TEST("EventPayload: overwrite existing key");
  EventPayload p;
  p.set("key", std::string("old"));
  p.set("key", std::string("new"));
  ASSERT_EQ(p.getString("key"), "new");
  ASSERT_EQ(p.size(), 1u);
  PASS();
}

// ==================== EventMetadata Tests ====================

static void test_metadata_limits() {
  TEST("EventMetadata: respects max entries");
  EventMetadata m;
  for (std::size_t i = 0; i < EventMetadata::kMaxEntries; ++i) {
    ASSERT_TRUE(m.set("key_" + std::to_string(i), static_cast<std::int64_t>(i)));
  }
  ASSERT_FALSE(m.set("overflow", static_cast<std::int64_t>(999)));
  ASSERT_EQ(m.size(), EventMetadata::kMaxEntries);
  PASS();
}

// ==================== EventBuilder Tests ====================

static void test_builder_valid_event() {
  TEST("EventBuilder: valid event builds");
  auto result = EventBuilder()
    .type("filesystem", "modified")
    .severity(EventSeverity::Info)
    .source(SourceRef{"win.fs", "FS", "1.0", SourceKind::OperatingSystem})
    .provenance(Provenance{"win.fs", "FS", "1.0"})
    .build();
  ASSERT_TRUE(std::holds_alternative<Event>(result));
  auto& e = std::get<Event>(result);
  ASSERT_TRUE(e.id.isValid());
  ASSERT_EQ(e.type.qualifiedName(), "filesystem.modified");
  PASS();
}

static void test_builder_missing_type() {
  TEST("EventBuilder: missing type returns error");
  auto result = EventBuilder()
    .severity(EventSeverity::Info)
    .source(SourceRef{"win.fs", "FS", "1.0"})
    .provenance(Provenance{"win.fs"})
    .build();
  ASSERT_TRUE(std::holds_alternative<EventErrorDetail>(result));
  ASSERT_EQ(std::get<EventErrorDetail>(result).code, EventError::MissingEventType);
  PASS();
}

static void test_builder_missing_source() {
  TEST("EventBuilder: missing source returns error");
  auto result = EventBuilder()
    .type("fs", "mod")
    .provenance(Provenance{"x"})
    .build();
  ASSERT_TRUE(std::holds_alternative<EventErrorDetail>(result));
  ASSERT_EQ(std::get<EventErrorDetail>(result).code, EventError::MissingSource);
  PASS();
}

static void test_builder_missing_provenance() {
  TEST("EventBuilder: missing provenance returns error");
  auto result = EventBuilder()
    .type("fs", "mod")
    .source(SourceRef{"x", "", "", SourceKind::Unknown})
    .build();
  ASSERT_TRUE(std::holds_alternative<EventErrorDetail>(result));
  ASSERT_EQ(std::get<EventErrorDetail>(result).code, EventError::MissingProvenance);
  PASS();
}

static void test_builder_without_actor_entity() {
  TEST("EventBuilder: success without actor/entity");
  auto result = EventBuilder()
    .type("fs", "mod")
    .source(SourceRef{"x", "", "", SourceKind::Unknown})
    .provenance(Provenance{"x"})
    .build();
  ASSERT_TRUE(std::holds_alternative<Event>(result));
  auto& e = std::get<Event>(result);
  ASSERT_FALSE(e.hasActor());
  ASSERT_FALSE(e.hasEntity());
  PASS();
}

static void test_builder_with_all_fields() {
  TEST("EventBuilder: all optional fields");
  auto result = EventBuilder()
    .type("process", "started")
    .severity(EventSeverity::Info)
    .source(SourceRef{"win.proc", "Proc", "1.0", SourceKind::OperatingSystem})
    .provenance(Provenance{"win.proc", "Proc", "1.0"})
    .actor(ActorRef{"process:4218", ActorKind::Process, std::nullopt, std::nullopt, 4218u})
    .entity(EntityRef{"process:4218", EntityKind::Process})
    .action("start")
    .flags(EventFlags::None)
    .build();
  ASSERT_TRUE(std::holds_alternative<Event>(result));
  auto& e = std::get<Event>(result);
  ASSERT_TRUE(e.hasActor());
  ASSERT_TRUE(e.hasEntity());
  ASSERT_TRUE(e.action->isValid());
  PASS();
}

// ==================== EventFactory Tests ====================

static void test_factory_simple() {
  TEST("EventFactory: createSimple");
  SourceRef src{"win.fs", "FS", "1.0", SourceKind::OperatingSystem};
  Event e = EventFactory::createSimple("filesystem", "modified", src);
  ASSERT_TRUE(e.id.isValid());
  ASSERT_EQ(e.type.qualifiedName(), "filesystem.modified");
  ASSERT_TRUE(e.source.isValid());
  ASSERT_TRUE(e.provenance.isValid());
  PASS();
}

static void test_factory_with_payload() {
  TEST("EventFactory: createWithPayload");
  SourceRef src{"win.fs", "FS", "1.0", SourceKind::OperatingSystem};
  EventPayload p;
  p.set("path", std::string("C:\\test.txt"));
  Event e = EventFactory::createWithPayload("filesystem", "created", src, std::move(p));
  ASSERT_TRUE(e.hasPayload());
  ASSERT_EQ(e.payload.getString("path"), "C:\\test.txt");
  PASS();
}

static void test_factory_derived() {
  TEST("EventFactory: createDerived");
  SourceRef src{"win.fs", "FS", "1.0", SourceKind::OperatingSystem};
  Provenance prov{"win.fs", "FS", "1.0"};
  EventId parentId = EventId::generate();
  Event e = EventFactory::createDerived("analysis", "activity_detected", src, prov, parentId);
  ASSERT_TRUE(e.isDerived());
  ASSERT_TRUE(e.correlation.parent_event_id.has_value());
  ASSERT_EQ(*e.correlation.parent_event_id, parentId);
  PASS();
}

// ==================== Event Immutability Tests ====================

static void test_event_is_value_type() {
  TEST("Event: value semantics (copy)");
  Event e1 = EventFactory::createSimple("fs", "mod", SourceRef{"x", "", "", SourceKind::Unknown});
  Event e2 = e1;
  ASSERT_EQ(e1.id, e2.id);
  ASSERT_EQ(e1.type, e2.type);
  PASS();
}

// ==================== Event Time Tests ====================

static void test_event_time_valid() {
  TEST("EventTime: validity check");
  EventTime t{1000, 1005};
  ASSERT_TRUE(t.isValid());
  ASSERT_NEAR(t.latencyMs(), 5.0, 0.001);
  PASS();
}

static void test_event_time_invalid() {
  TEST("EventTime: zero is invalid");
  EventTime t{0, 0};
  ASSERT_FALSE(t.isValid());
  PASS();
}

// ==================== MonotonicTime Tests ====================

static void test_monotonic_now() {
  TEST("MonotonicTime: now returns non-zero");
  auto t = MonotonicTime::now();
  ASSERT_TRUE(t.nanoseconds > 0);
  PASS();
}

// ==================== Serialization Tests ====================

static void test_serializer_roundtrip() {
  TEST("Serialization: full roundtrip");
  auto id = EventId::generate();

  SourceRef src{"win.fs", "FS Collector", "1.0", SourceKind::OperatingSystem};
  ActorRef actor{"process:4218", ActorKind::Process, std::nullopt, std::nullopt, 4218u};
  EntityRef entity{"file:928183", EntityKind::File, std::string("config.json")};
  Action action{"modify"};
  Provenance prov{"win.fs", "FS Collector", "1.0", "monix.event", 1, ProvenanceKind::NativeObservation};
  Correlation corr;
  corr.activity_id = "activity:8821";

  EventPayload payload;
  payload.set("path", std::string("C:\\Project\\config.json"));
  payload.set("size", static_cast<std::int64_t>(18321));

  EventMetadata meta;
  meta.set("platform_notification", static_cast<std::int64_t>(829182));

  EventBuilder builder;
  builder.type("filesystem", "modified")
    .severity(EventSeverity::Info)
    .source(src)
    .actor(actor)
    .entity(entity)
    .action(action)
    .correlation(corr)
    .provenance(prov)
    .payload(std::move(payload))
    .metadata(std::move(meta))
    .flags(EventFlags::None);

  auto result = builder.build();
  ASSERT_TRUE(std::holds_alternative<Event>(result));
  Event event = std::get<Event>(std::move(result));

  std::string json = EventSerializer::serialize(event);
  ASSERT_TRUE(json.size() > 10);

  auto deserialized = EventDeserializer::deserialize(json);
  ASSERT_TRUE(std::holds_alternative<Event>(deserialized));
  Event& e2 = std::get<Event>(deserialized);

  ASSERT_EQ(event.id, e2.id);
  ASSERT_EQ(event.type, e2.type);
  ASSERT_EQ(event.severity, e2.severity);
  ASSERT_EQ(event.source.id, e2.source.id);
  ASSERT_TRUE(e2.hasActor());
  ASSERT_EQ(e2.actor->id, "process:4218");
  ASSERT_TRUE(e2.hasEntity());
  ASSERT_EQ(e2.entity->id, "file:928183");
  ASSERT_TRUE(e2.hasPayload());
  ASSERT_EQ(e2.payload.getString("path"), "C:\\Project\\config.json");
  ASSERT_EQ(e2.payload.getInt("size"), 18321);
  PASS();
}

static void test_serializer_minimal() {
  TEST("Serialization: minimal event");
  Event e = EventFactory::createSimple("system", "heartbeat",
    SourceRef{"monix.core", "Core", "1.0", SourceKind::MonixComponent});

  std::string json = EventSerializer::serialize(e);
  auto e2 = EventDeserializer::deserialize(json);
  ASSERT_TRUE(std::holds_alternative<Event>(e2));

  auto& ed = std::get<Event>(e2);
  ASSERT_EQ(ed.type.qualifiedName(), "system.heartbeat");
  ASSERT_TRUE(ed.source.isValid());
  PASS();
}

// ==================== Corruption Tests ====================

static void test_deserialize_invalid_json() {
  TEST("Deserialization: corrupt JSON returns error");
  auto r = EventDeserializer::deserialize("{invalid json!!!");
  ASSERT_TRUE(std::holds_alternative<EventErrorDetail>(r));
  PASS();
}

static void test_deserialize_empty() {
  TEST("Deserialization: empty string returns error");
  auto r = EventDeserializer::deserialize("");
  ASSERT_TRUE(std::holds_alternative<EventErrorDetail>(r));
  PASS();
}

static void test_deserialize_null_type() {
  TEST("Deserialization: null type returns error");
  auto r = EventDeserializer::deserialize("{\"type\": null}");
  ASSERT_TRUE(std::holds_alternative<EventErrorDetail>(r));
  PASS();
}

static void test_deserialize_missing_required() {
  TEST("Deserialization: missing id returns error");
  auto r = EventDeserializer::deserialize("{\"type\": {\"namespace\": \"x\", \"name\": \"y\"}}");
  ASSERT_TRUE(std::holds_alternative<EventErrorDetail>(r));
  PASS();
}

// ==================== Error Model Tests ====================

static void test_error_names() {
  TEST("ErrorModel: all error names non-null");
  ASSERT_TRUE(EventErrorName(EventError::None) != nullptr);
  ASSERT_TRUE(EventErrorName(EventError::InvalidEventId) != nullptr);
  ASSERT_TRUE(EventErrorName(EventError::EventTooLarge) != nullptr);
  ASSERT_TRUE(EventErrorDescription(EventError::PayloadTooLarge) != nullptr);
  PASS();
}

// ==================== Unknown Semantics ====================

static void test_unknown_vs_null() {
  TEST("Unknown vs NULL semantics");
  ActorRef a;
  a.id = "unknown_actor";
  a.kind = ActorKind::Unknown;
  ASSERT_TRUE(a.isValid());
  ASSERT_EQ(a.kind, ActorKind::Unknown);
  ASSERT_FALSE(a.process_id.has_value());
  PASS();
}

// ==================== Concurrency Tests ====================

static void test_concurrent_id_generation() {
  TEST("Concurrency: 16 threads x 10K IDs unique");
  std::set<std::string> allIds;
  std::mutex mu;
  std::vector<std::thread> threads;

  for (int t = 0; t < 16; ++t) {
    threads.emplace_back([&]() {
      std::set<std::string> local;
      for (int i = 0; i < 10000; ++i) {
        local.insert(EventId::generate().toString());
      }
      std::lock_guard<std::mutex> lock(mu);
      for (const auto& id : local) {
        if (allIds.count(id)) {
          printf("[FAIL] duplicate in concurrent test\n");
          gFailed++;
          return;
        }
      }
      allIds.insert(local.begin(), local.end());
    });
  }
  for (auto& th : threads) th.join();
  ASSERT_EQ(allIds.size(), 160000u);
  PASS();
}

// ==================== Performance Baseline ====================

static void test_perf_event_creation() {
  TEST("Perf: 1M events creation");
  SourceRef src{"test", "Test", "1.0", SourceKind::Synthetic};
  auto start = std::chrono::steady_clock::now();
  for (int i = 0; i < 1000000; ++i) {
    EventFactory::createSimple("test", "perf", src);
  }
  auto end = std::chrono::steady_clock::now();
  double ms = std::chrono::duration<double, std::milli>(end - start).count();
  printf("(%.1f ms) ", ms);
  PASS();
}

static void test_perf_serialization() {
  TEST("Perf: 100K serialize/deserialize roundtrips");
  SourceRef src{"test", "Test", "1.0", SourceKind::Synthetic};
  Event e = EventFactory::createWithPayload("test", "perf", src,
    []{ EventPayload p; p.set("v", static_cast<std::int64_t>(42)); return p; }());

  auto start = std::chrono::steady_clock::now();
  for (int i = 0; i < 100000; ++i) {
    std::string json = EventSerializer::serialize(e);
    auto r = EventDeserializer::deserialize(json);
    (void)r;
  }
  auto end = std::chrono::steady_clock::now();
  double ms = std::chrono::duration<double, std::milli>(end - start).count();
  printf("(%.1f ms) ", ms);
  PASS();
}

// ==================== Main ====================

#ifndef MONIX_KERNEL_BUILD
int main() {
  printf("=== MONIX Event Core Tests ===\n\n");

  printf("[EventId]\n");
  test_eventid_generate_unique();
  test_eventid_serialization_roundtrip();
  test_eventid_bytes_roundtrip();
  test_eventid_invalid_string();
  test_eventid_empty();
  test_eventid_format();

  printf("\n[Core Types]\n");
  test_eventtype_valid();
  test_eventtype_invalid_empty();
  test_severity_names();
  test_severity_from_string();
  test_flags_operators();
  test_event_time_valid();
  test_event_time_invalid();
  test_monotonic_now();

  printf("\n[References]\n");
  test_sourceref_valid();
  test_sourceref_invalid();
  test_actorref_optional_fields();
  test_entityref_optional();
  test_action_valid();
  test_action_with_id();
  test_correlation_hasany();
  test_provenance_valid();
  test_provenance_kind_names();

  printf("\n[Payload & Metadata]\n");
  test_payload_set_get();
  test_payload_overwrite();
  test_metadata_limits();

  printf("\n[Builder]\n");
  test_builder_valid_event();
  test_builder_missing_type();
  test_builder_missing_source();
  test_builder_missing_provenance();
  test_builder_without_actor_entity();
  test_builder_with_all_fields();

  printf("\n[Factory]\n");
  test_factory_simple();
  test_factory_with_payload();
  test_factory_derived();

  printf("\n[Semantics]\n");
  test_event_is_value_type();
  test_unknown_vs_null();

  printf("\n[Serialization]\n");
  test_serializer_roundtrip();
  test_serializer_minimal();

  printf("\n[Corruption]\n");
  test_deserialize_invalid_json();
  test_deserialize_empty();
  test_deserialize_null_type();
  test_deserialize_missing_required();

  printf("\n[Error Model]\n");
  test_error_names();

  printf("\n[Concurrency]\n");
  test_concurrent_id_generation();

  printf("\n[Performance]\n");
  test_perf_event_creation();
  test_perf_serialization();

  printf("\n=== Results: %d passed, %d failed ===\n", gPassed, gFailed);
  return gFailed > 0 ? 1 : 0;
}
#endif

#ifdef MONIX_KERNEL_BUILD
int GetFailedCount_EventCoreTests() { return gFailed; }

struct KTestEntry {
  const char* display_name;
  void (*func)();
};

static const KTestEntry s_ktests[] = {
  {"EventId: 1M unique", test_eventid_generate_unique},
  {"EventId: serialization roundtrip", test_eventid_serialization_roundtrip},
  {"EventId: bytes roundtrip", test_eventid_bytes_roundtrip},
  {"EventId: invalid string", test_eventid_invalid_string},
  {"EventId: empty", test_eventid_empty},
  {"EventId: format", test_eventid_format},
  {"EventType: valid", test_eventtype_valid},
  {"EventType: invalid empty", test_eventtype_invalid_empty},
  {"Severity: names", test_severity_names},
  {"Severity: from string", test_severity_from_string},
  {"Flags: operators", test_flags_operators},
  {"SourceRef: valid", test_sourceref_valid},
  {"SourceRef: invalid", test_sourceref_invalid},
  {"ActorRef: optional fields", test_actorref_optional_fields},
  {"EntityRef: optional", test_entityref_optional},
  {"Action: valid", test_action_valid},
  {"Action: with id", test_action_with_id},
  {"Correlation: hasAny", test_correlation_hasany},
  {"Provenance: valid", test_provenance_valid},
  {"ProvenanceKind: names", test_provenance_kind_names},
  {"Payload: set get", test_payload_set_get},
  {"Payload: overwrite", test_payload_overwrite},
  {"Metadata: limits", test_metadata_limits},
  {"Builder: valid event", test_builder_valid_event},
  {"Builder: missing type", test_builder_missing_type},
  {"Builder: missing source", test_builder_missing_source},
  {"Builder: missing provenance", test_builder_missing_provenance},
  {"Builder: without actor entity", test_builder_without_actor_entity},
  {"Builder: with all fields", test_builder_with_all_fields},
  {"Factory: simple", test_factory_simple},
  {"Factory: with payload", test_factory_with_payload},
  {"Factory: derived", test_factory_derived},
  {"Event: value type", test_event_is_value_type},
  {"EventTime: valid", test_event_time_valid},
  {"EventTime: invalid", test_event_time_invalid},
  {"MonotonicTime: now", test_monotonic_now},
  {"Serialization: roundtrip", test_serializer_roundtrip},
  {"Serialization: minimal", test_serializer_minimal},
  {"Deserialization: invalid json", test_deserialize_invalid_json},
  {"Deserialization: empty", test_deserialize_empty},
  {"Deserialization: null type", test_deserialize_null_type},
  {"Deserialization: missing required", test_deserialize_missing_required},
  {"ErrorModel: names", test_error_names},
  {"Unknown vs NULL", test_unknown_vs_null},
  {"Concurrency: 16 threads", test_concurrent_id_generation},
  {"Perf: 1M events", test_perf_event_creation},
  {"Perf: 100K serialize", test_perf_serialization},
};

const KTestEntry* GetKTests_EventCore() { return s_ktests; }
std::size_t GetKTestCount_EventCore() { return sizeof(s_ktests) / sizeof(s_ktests[0]); }
#endif
