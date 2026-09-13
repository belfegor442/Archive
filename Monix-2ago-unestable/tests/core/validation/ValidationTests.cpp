#include "../../core/validation/ValidationTypes.hpp"
#include "../../core/validation/ValidationResult.hpp"
#include "../../core/validation/ValidationPolicy.hpp"
#include "../../core/validation/ValidationContext.hpp"
#include "../../core/validation/EventSchema.hpp"
#include "../../core/validation/EventSchemaRegistry.hpp"
#include "../../core/validation/SourceRegistry.hpp"
#include "../../core/validation/StructuralValidator.hpp"
#include "../../core/validation/SchemaValidator.hpp"
#include "../../core/validation/SemanticValidator.hpp"
#include "../../core/validation/IntegrityValidator.hpp"
#include "../../core/validation/EventValidationPipeline.hpp"
#include "../../core/validation/QuarantineManager.hpp"
#include "../../core/validation/ValidationMetrics.hpp"

#include "../../core/events/EventFactory.hpp"
#include "../../core/events/EventId.hpp"

#include <cassert>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>
#include <chrono>
#include <cmath>

using namespace monix::events;
using namespace monix::validation;

static int gPassed = 0;
static int gFailed = 0;

#define TEST(name) printf("  %-60s ", name);
#define PASS() do { printf("[PASS]\n"); gPassed++; } while(0)
#define FAIL(msg) do { printf("[FAIL] %s\n", msg); gFailed++; } while(0)
#define ASSERT_TRUE(e) do { if (!(e)) { FAIL(#e); return; } } while(0)
#define ASSERT_FALSE(e) do { if ((e)) { FAIL(#e); return; } } while(0)
#define ASSERT_EQ(a, b) do { if ((a) != (b)) { FAIL(#a " != " #b); return; } } while(0)
#define ASSERT_NE(a, b) do { if ((a) == (b)) { FAIL(#a " == " #b); return; } } while(0)
#define ASSERT_GE(a, b) do { if ((a) < (b)) { FAIL(#a " < " #b); return; } } while(0)
#define ASSERT_NEAR(a, b, e) do { if (std::abs((a)-(b)) > (e)) { FAIL(#a " !~= " #b); return; } } while(0)

static SourceRef testSource() {
  return {"collector.filesystem", "FS", "1.0", SourceKind::MonixCollector};
}

static Provenance testProvenance() {
  return {"collector.filesystem", "FS", "1.0", "monix.event", 1, ProvenanceKind::CollectorObservation};
}

static Event makeValidEvent() {
  Event e = EventFactory::createSimple("filesystem", "modified", testSource(), EventSeverity::Info);
  e.time.occurrence = currentSystemTimeMs();
  e.time.ingestion = currentSystemTimeMs();
  e.provenance = testProvenance();
  return e;
}

static Event makeMinimalEvent() {
  Event e;
  e.id = EventId::generate();
  e.type = {"filesystem", "modified"};
  e.time.occurrence = currentSystemTimeMs();
  e.time.ingestion = currentSystemTimeMs();
  e.source = testSource();
  e.provenance = testProvenance();
  return e;
}

// ==================== Type Names Tests ====================

static void test_status_names() {
  TEST("ValidationStatus: all names");
  ASSERT_EQ(std::string(ValidationStatusName(ValidationStatus::Valid)), "Valid");
  ASSERT_EQ(std::string(ValidationStatusName(ValidationStatus::ValidWithWarnings)), "ValidWithWarnings");
  ASSERT_EQ(std::string(ValidationStatusName(ValidationStatus::Invalid)), "Invalid");
  PASS();
}

static void test_severity_names() {
  TEST("ValidationSeverity: all names");
  ASSERT_EQ(std::string(ValidationSeverityName(ValidationSeverity::Notice)), "Notice");
  ASSERT_EQ(std::string(ValidationSeverityName(ValidationSeverity::Warning)), "Warning");
  ASSERT_EQ(std::string(ValidationSeverityName(ValidationSeverity::Error)), "Error");
  ASSERT_EQ(std::string(ValidationSeverityName(ValidationSeverity::Fatal)), "Fatal");
  PASS();
}

static void test_issue_code_names() {
  TEST("ValidationIssueCode: all names non-null");
  for (int i = 1; i <= 33; ++i) {
    const char* name = ValidationIssueCodeName(static_cast<ValidationIssueCode>(i));
    if (!name || std::string(name) == "Unknown") {
      FAIL("Issue code name is null/Unknown");
      return;
    }
  }
  PASS();
}

static void test_profile_names() {
  TEST("ValidationProfile: all names");
  ASSERT_EQ(std::string(ValidationProfileName(ValidationProfile::Fast)), "Fast");
  ASSERT_EQ(std::string(ValidationProfileName(ValidationProfile::Standard)), "Standard");
  ASSERT_EQ(std::string(ValidationProfileName(ValidationProfile::Strict)), "Strict");
  ASSERT_EQ(std::string(ValidationProfileName(ValidationProfile::Forensic)), "Forensic");
  PASS();
}

// ==================== Policy Tests ====================

static void test_policy_presets() {
  TEST("ValidationPolicy: preset configurations");
  auto fast = ValidationPolicy::fast();
  ASSERT_EQ(fast.profile, ValidationProfile::Fast);
  ASSERT_FALSE(fast.verify_integrity);

  auto std = ValidationPolicy::standard();
  ASSERT_EQ(std.profile, ValidationProfile::Standard);

  auto strict = ValidationPolicy::strict();
  ASSERT_TRUE(strict.reject_unknown_schema);
  ASSERT_TRUE(strict.verify_integrity);

  auto forensic = ValidationPolicy::forensic();
  ASSERT_TRUE(forensic.verify_integrity);
  ASSERT_TRUE(forensic.verify_sequence);
  PASS();
}

// ==================== ValidationResult Tests ====================

static void test_result_valid() {
  TEST("ValidationResult: valid has no issues");
  ValidationResult r;
  ASSERT_TRUE(r.isValid());
  ASSERT_FALSE(r.hasWarnings());
  ASSERT_FALSE(r.isInvalid());
  PASS();
}

static void test_result_merge_worsens() {
  TEST("ValidationResult: mergeFrom worsens status");
  ValidationResult a;
  a.status = ValidationStatus::Valid;

  ValidationResult b;
  b.status = ValidationStatus::ValidWithWarnings;
  b.addIssue(ValidationIssueCode::UnknownField, ValidationSeverity::Notice, "x", "test");

  a.mergeFrom(b);
  ASSERT_EQ(a.status, ValidationStatus::ValidWithWarnings);
  ASSERT_EQ(a.issues.size(), 1u);
  PASS();
}

static void test_result_merge_invalid() {
  TEST("ValidationResult: mergeFrom Invalid dominates");
  ValidationResult a;
  a.status = ValidationStatus::ValidWithWarnings;

  ValidationResult b;
  b.status = ValidationStatus::Invalid;
  b.addIssue(ValidationIssueCode::MissingEventId, ValidationSeverity::Fatal, "id", "missing");

  a.mergeFrom(b);
  ASSERT_EQ(a.status, ValidationStatus::Invalid);
  PASS();
}

static void test_result_add_issue() {
  TEST("ValidationResult: addIssue stores correctly");
  ValidationResult r;
  r.addIssue(ValidationIssueCode::MissingRequiredField, ValidationSeverity::Error,
             "path", "Required field missing");
  ASSERT_EQ(r.issues.size(), 1u);
  ASSERT_EQ(r.issues[0].code, ValidationIssueCode::MissingRequiredField);
  ASSERT_EQ(r.issues[0].severity, ValidationSeverity::Error);
  ASSERT_EQ(r.issues[0].field, "path");
  PASS();
}

// ==================== Structural Validator Tests ====================

static void test_structural_valid_event() {
  TEST("Structural: valid event passes");
  StructuralValidator sv;
  ValidationContext ctx;
  auto event = makeValidEvent();
  auto result = sv.validate(event, ctx);
  ASSERT_EQ(result.status, ValidationStatus::Valid);
  ASSERT_TRUE(result.issues.empty());
  PASS();
}

static void test_structural_missing_id() {
  TEST("Structural: missing EventId is Fatal");
  StructuralValidator sv;
  ValidationContext ctx;
  auto event = makeValidEvent();
  event.id = EventId{};
  auto result = sv.validate(event, ctx);
  ASSERT_EQ(result.status, ValidationStatus::Invalid);
  bool found = false;
  for (const auto& i : result.issues) {
    if (i.code == ValidationIssueCode::MissingEventId) found = true;
  }
  ASSERT_TRUE(found);
  PASS();
}

static void test_structural_missing_type() {
  TEST("Structural: missing type is Fatal");
  StructuralValidator sv;
  ValidationContext ctx;
  auto event = makeValidEvent();
  event.type = EventType{};
  auto result = sv.validate(event, ctx);
  ASSERT_EQ(result.status, ValidationStatus::Invalid);
  PASS();
}

static void test_structural_missing_timestamp() {
  TEST("Structural: missing timestamp is Fatal");
  StructuralValidator sv;
  ValidationContext ctx;
  auto event = makeValidEvent();
  event.time = EventTime{};
  auto result = sv.validate(event, ctx);
  ASSERT_EQ(result.status, ValidationStatus::Invalid);
  PASS();
}

static void test_structural_missing_source() {
  TEST("Structural: missing source is Fatal");
  StructuralValidator sv;
  ValidationContext ctx;
  auto event = makeValidEvent();
  event.source = SourceRef{};
  auto result = sv.validate(event, ctx);
  ASSERT_EQ(result.status, ValidationStatus::Invalid);
  PASS();
}

static void test_structural_missing_provenance() {
  TEST("Structural: missing provenance is Error");
  StructuralValidator sv;
  ValidationContext ctx;
  auto event = makeValidEvent();
  event.provenance = Provenance{};
  auto result = sv.validate(event, ctx);
  bool found = false;
  for (const auto& i : result.issues) {
    if (i.code == ValidationIssueCode::MissingProvenance) found = true;
  }
  ASSERT_TRUE(found);
  PASS();
}

static void test_structural_multiple_fatal() {
  TEST("Structural: multiple fatals detected");
  StructuralValidator sv;
  ValidationContext ctx;
  Event event;
  auto result = sv.validate(event, ctx);
  ASSERT_EQ(result.status, ValidationStatus::Invalid);
  ASSERT_GE(result.issues.size(), 3u);
  PASS();
}

// ==================== Schema Validator Tests ====================

static void test_schema_no_registry_passes() {
  TEST("Schema: no registry returns Valid");
  SchemaValidator sv;
  ValidationContext ctx;
  ctx.schemas = nullptr;
  auto event = makeValidEvent();
  auto result = sv.validate(event, ctx);
  ASSERT_EQ(result.status, ValidationStatus::Valid);
  PASS();
}

static void test_schema_unknown_reject() {
  TEST("Schema: unknown schema rejected when policy requires");
  EventSchemaRegistry registry;
  SchemaValidator sv;
  ValidationContext ctx;
  ctx.schemas = &registry;
  ctx.policy.reject_unknown_schema = true;
  auto event = makeValidEvent();
  auto result = sv.validate(event, ctx);
  ASSERT_EQ(result.status, ValidationStatus::Invalid);
  PASS();
}

static void test_schema_unknown_warn() {
  TEST("Schema: unknown schema warned when policy allows");
  EventSchemaRegistry registry;
  SchemaValidator sv;
  ValidationContext ctx;
  ctx.schemas = &registry;
  ctx.policy.reject_unknown_schema = false;
  auto event = makeValidEvent();
  auto result = sv.validate(event, ctx);
  ASSERT_EQ(result.status, ValidationStatus::Valid);
  PASS();
}

static void test_schema_found_passes() {
  TEST("Schema: registered schema passes basic check");
  EventSchemaRegistry registry;
  EventSchema schema;
  schema.name = "filesystem.modified";
  schema.version = 1;
  schema.type = {"filesystem", "modified"};
  registry.registerSchema(std::move(schema));

  SchemaValidator sv;
  ValidationContext ctx;
  ctx.schemas = &registry;
  auto event = makeValidEvent();
  auto result = sv.validate(event, ctx);
  ASSERT_EQ(result.status, ValidationStatus::Valid);
  PASS();
}

static void test_schema_unknown_fields_warn() {
  TEST("Schema: unknown payload fields generate warning");
  EventSchemaRegistry registry;
  EventSchema schema;
  schema.name = "filesystem.modified";
  schema.version = 1;
  schema.type = {"filesystem", "modified"};
  schema.required.push_back({"path", FieldType::String, true});
  registry.registerSchema(std::move(schema));

  SchemaValidator sv;
  ValidationContext ctx;
  ctx.schemas = &registry;
  ctx.policy.unknown_fields = DropPolicy::Warn;
  auto event = makeValidEvent();
  event.payload.set("path", std::string("/tmp/test"));
  event.payload.set("weird_field", std::string("surprise"));
  auto result = sv.validate(event, ctx);
  bool found = false;
  for (const auto& i : result.issues) {
    if (i.code == ValidationIssueCode::UnknownField) found = true;
  }
  ASSERT_TRUE(found);
  PASS();
}

// ==================== Semantic Validator Tests ====================

static void test_semantic_valid_event() {
  TEST("Semantic: valid event passes");
  SemanticValidator sv;
  ValidationContext ctx;
  ctx.now = currentSystemTimeMs();
  auto event = makeValidEvent();
  auto result = sv.validate(event, ctx);
  ASSERT_TRUE(result.isValid());
  PASS();
}

static void test_semantic_future_timestamp() {
  TEST("Semantic: far future timestamp generates warning");
  SemanticValidator sv;
  ValidationContext ctx;
  ctx.now = currentSystemTimeMs();
  ctx.policy.max_future_skew = std::chrono::seconds(5);
  auto event = makeValidEvent();
  event.time.occurrence = ctx.now + 600000;
  auto result = sv.validate(event, ctx);
  bool found = false;
  for (const auto& i : result.issues) {
    if (i.code == ValidationIssueCode::FutureTimestamp) found = true;
  }
  ASSERT_TRUE(found);
  PASS();
}

static void test_semantic_clock_skew() {
  TEST("Semantic: clock skew detected");
  SemanticValidator sv;
  ValidationContext ctx;
  ctx.now = currentSystemTimeMs();
  ctx.policy.max_future_skew = std::chrono::seconds(5);
  auto event = makeValidEvent();
  event.time.occurrence = ctx.now;
  event.time.ingestion = ctx.now + 600000;
  auto result = sv.validate(event, ctx);
  bool found = false;
  for (const auto& i : result.issues) {
    if (i.code == ValidationIssueCode::ClockSkewDetected) found = true;
  }
  ASSERT_TRUE(found);
  PASS();
}

static void test_semantic_duplicate_id() {
  TEST("Semantic: duplicate event ID detected");
  SemanticValidator sv;
  ValidationContext ctx;
  ctx.now = currentSystemTimeMs();
  auto event = makeValidEvent();
  sv.validate(event, ctx);
  auto result = sv.validate(event, ctx);
  bool found = false;
  for (const auto& i : result.issues) {
    if (i.code == ValidationIssueCode::DuplicateEventId) found = true;
  }
  ASSERT_TRUE(found);
  PASS();
}

static void test_semantic_user_actor_no_username() {
  TEST("Semantic: User actor without username generates notice");
  SemanticValidator sv;
  ValidationContext ctx;
  ctx.now = currentSystemTimeMs();
  ctx.policy.profile = ValidationProfile::Standard;
  auto event = makeValidEvent();
  ActorRef actor;
  actor.id = "user ori";
  actor.kind = ActorKind::User;
  event.actor = actor;
  auto result = sv.validate(event, ctx);
  bool found = false;
  for (const auto& i : result.issues) {
    if (i.code == ValidationIssueCode::InvalidActor) found = true;
  }
  ASSERT_TRUE(found);
  PASS();
}

static void test_semantic_file_empty_name() {
  TEST("Semantic: File entity with empty name is Error");
  SemanticValidator sv;
  ValidationContext ctx;
  ctx.now = currentSystemTimeMs();
  auto event = makeValidEvent();
  EntityRef entity;
  entity.id = "file1";
  entity.kind = EntityKind::File;
  entity.name = std::string("");
  event.entity = entity;
  auto result = sv.validate(event, ctx);
  ASSERT_EQ(result.status, ValidationStatus::Invalid);
  PASS();
}

static void test_semantic_action_type_conflict() {
  TEST("Semantic: action contradicts event type");
  SemanticValidator sv;
  ValidationContext ctx;
  ctx.now = currentSystemTimeMs();
  auto event = makeValidEvent();
  event.type = {"filesystem", "created"};
  Action action;
  action.name = "delete";
  event.action = action;
  auto result = sv.validate(event, ctx);
  ASSERT_EQ(result.status, ValidationStatus::Invalid);
  PASS();
}

static void test_semantic_clear_seen_ids() {
  TEST("Semantic: clearSeenIds resets duplicate tracking");
  SemanticValidator sv;
  ValidationContext ctx;
  ctx.now = currentSystemTimeMs();
  auto event = makeValidEvent();
  sv.validate(event, ctx);
  sv.clearSeenIds();
  auto result = sv.validate(event, ctx);
  bool found = false;
  for (const auto& i : result.issues) {
    if (i.code == ValidationIssueCode::DuplicateEventId) found = true;
  }
  ASSERT_FALSE(found);
  PASS();
}

// ==================== Integrity Validator Tests ====================

static void test_integrity_canonicalize() {
  TEST("Integrity: canonicalize produces deterministic output");
  auto event = makeValidEvent();
  std::string c1 = IntegrityValidator::canonicalize(event);
  std::string c2 = IntegrityValidator::canonicalize(event);
  ASSERT_EQ(c1, c2);
  ASSERT_TRUE(c1.find("filesystem.modified") != std::string::npos);
  PASS();
}

static void test_integrity_hash_deterministic() {
  TEST("Integrity: hash is deterministic");
  auto event = makeValidEvent();
  auto h1 = IntegrityValidator::computeHash(event);
  auto h2 = IntegrityValidator::computeHash(event);
  ASSERT_FALSE(h1.empty());
  ASSERT_EQ(h1.hex(), h2.hex());
  PASS();
}

static void test_integrity_hash_differs_on_modify() {
  TEST("Integrity: hash changes when event changes");
  auto event1 = makeValidEvent();
  auto event2 = event1;
  event2.sequence = 999;
  auto h1 = IntegrityValidator::computeHash(event1);
  auto h2 = IntegrityValidator::computeHash(event2);
  ASSERT_NE(h1.hex(), h2.hex());
  PASS();
}

static void test_integrity_disabled_passes() {
  TEST("Integrity: disabled by policy passes silently");
  IntegrityValidator iv;
  ValidationContext ctx;
  ctx.policy.verify_integrity = false;
  auto event = makeValidEvent();
  auto result = iv.validate(event, ctx);
  ASSERT_EQ(result.status, ValidationStatus::Valid);
  ASSERT_TRUE(result.issues.empty());
  PASS();
}

static void test_integrity_enabled_computes() {
  TEST("Integrity: enabled computes hash");
  IntegrityValidator iv;
  ValidationContext ctx;
  ctx.policy.verify_integrity = true;
  auto event = makeValidEvent();
  auto result = iv.validate(event, ctx);
  ASSERT_EQ(result.status, ValidationStatus::Valid);
  PASS();
}

// ==================== Pipeline Tests ====================

static void test_pipeline_valid_event() {
  TEST("Pipeline: valid event passes all validators");
  EventValidationPipeline pipeline;
  auto event = makeValidEvent();
  auto result = pipeline.validate(event);
  ASSERT_TRUE(result.isValid());
  PASS();
}

static void test_pipeline_missing_id_fails() {
  TEST("Pipeline: missing ID stops at structural");
  EventValidationPipeline pipeline;
  auto event = makeValidEvent();
  event.id = EventId{};
  auto result = pipeline.validate(event);
  ASSERT_FALSE(result.isValid());
  PASS();
}

static void test_pipeline_to_report() {
  TEST("Pipeline: toReport creates proper report");
  EventValidationPipeline pipeline;
  auto event = makeValidEvent();
  auto result = pipeline.validate(event);
  auto report = pipeline.toReport(event, result);
  ASSERT_EQ(report.status, ValidationStatus::Valid);
  ASSERT_FALSE(report.event_id.empty());
  PASS();
}

static void test_pipeline_with_schema() {
  TEST("Pipeline: schema validation integrated");
  EventSchemaRegistry schemas;
  EventSchema schema;
  schema.name = "filesystem.modified";
  schema.version = 1;
  schema.type = {"filesystem", "modified"};
  schemas.registerSchema(std::move(schema));

  EventValidationPipeline pipeline;
  pipeline.setSchemaRegistry(&schemas);
  auto event = makeValidEvent();
  auto result = pipeline.validate(event);
  ASSERT_TRUE(result.isValid());
  PASS();
}

// ==================== Quarantine Tests ====================

static void test_quarantine_basic() {
  TEST("Quarantine: basic quarantine and drain");
  QuarantineManager qm;
  auto event = makeValidEvent();
  ValidationResult vr;
  vr.status = ValidationStatus::Invalid;
  vr.addIssue(ValidationIssueCode::MissingEventId, ValidationSeverity::Fatal, "id", "missing");

  qm.quarantine(event, vr, currentSystemTimeMs());
  ASSERT_EQ(qm.size(), 1u);
  ASSERT_EQ(qm.totalQuarantined(), 1u);

  auto entries = qm.drain();
  ASSERT_EQ(entries.size(), 1u);
  ASSERT_EQ(qm.size(), 0u);
  PASS();
}

static void test_quarantine_limits() {
  TEST("Quarantine: respects max_events limit");
  QuarantineConfig cfg;
  cfg.max_events = 10;
  QuarantineManager qm(cfg);

  for (int i = 0; i < 15; ++i) {
    auto event = makeValidEvent();
    ValidationResult vr;
    vr.status = ValidationStatus::Invalid;
    vr.addIssue(ValidationIssueCode::MissingEventId, ValidationSeverity::Fatal, "id", "x");
    qm.quarantine(event, vr, currentSystemTimeMs());
  }
  ASSERT_EQ(qm.size(), 10u);
  ASSERT_EQ(qm.totalQuarantined(), 15u);
  PASS();
}

static void test_quarantine_aggregation() {
  TEST("Quarantine: repeated failures aggregate");
  QuarantineManager qm;
  Timestamp now = currentSystemTimeMs();

  for (int i = 0; i < 100; ++i) {
    auto event = makeValidEvent();
    ValidationResult vr;
    vr.status = ValidationStatus::Invalid;
    vr.addIssue(ValidationIssueCode::MissingRequiredField, ValidationSeverity::Error, "path", "x");
    qm.quarantine(event, vr, now + i);
  }

  auto aggs = qm.aggregates();
  ASSERT_EQ(aggs.size(), 1u);
  ASSERT_EQ(aggs[0].count, 100u);
  ASSERT_FALSE(aggs[0].representative_event_id.empty());
  PASS();
}

static void test_quarantine_purge_expired() {
  TEST("Quarantine: purgeExpired removes old entries");
  QuarantineConfig cfg;
  cfg.retention = std::chrono::seconds(60);
  QuarantineManager qm(cfg);

  Timestamp now = currentSystemTimeMs();
  auto event = makeValidEvent();
  ValidationResult vr;
  vr.status = ValidationStatus::Invalid;
  qm.quarantine(event, vr, now - 120000);

  ASSERT_EQ(qm.size(), 1u);
  qm.purgeExpired(now);
  ASSERT_EQ(qm.size(), 0u);
  PASS();
}

static void test_quarantine_clear() {
  TEST("Quarantine: clear resets everything");
  QuarantineManager qm;
  for (int i = 0; i < 5; ++i) {
    auto event = makeValidEvent();
    ValidationResult vr;
    vr.status = ValidationStatus::Invalid;
    qm.quarantine(event, vr, currentSystemTimeMs());
  }
  qm.clear();
  ASSERT_EQ(qm.size(), 0u);
  ASSERT_EQ(qm.aggregates().size(), 0u);
  PASS();
}

// ==================== Metrics Tests ====================

static void test_metrics_basic() {
  TEST("Metrics: basic tracking");
  ValidationMetricsTracker tracker;
  tracker.recordValidated(ValidationStatus::Valid, 10.0);
  tracker.recordValidated(ValidationStatus::Invalid, 20.0);
  tracker.recordValidated(ValidationStatus::ValidWithWarnings, 5.0);

  auto m = tracker.snapshot();
  ASSERT_EQ(m.events_validated, 3u);
  ASSERT_EQ(m.events_valid, 1u);
  ASSERT_EQ(m.events_invalid, 1u);
  ASSERT_EQ(m.events_warnings, 1u);
  PASS();
}

static void test_metrics_source_tracking() {
  TEST("Metrics: per-source tracking");
  ValidationMetricsTracker tracker;
  tracker.recordSource("fs", ValidationStatus::Valid);
  tracker.recordSource("fs", ValidationStatus::Invalid);
  tracker.recordSource("net", ValidationStatus::Valid);

  auto h = tracker.health("fs");
  ASSERT_EQ(h.events_received, 2u);
  ASSERT_EQ(h.valid, 1u);
  ASSERT_EQ(h.invalid, 1u);
  ASSERT_NEAR(h.invalid_rate, 0.5, 0.01);

  auto hNet = tracker.health("net");
  ASSERT_EQ(hNet.events_received, 1u);
  ASSERT_EQ(hNet.invalid_rate, 0.0);
  PASS();
}

static void test_metrics_latency_average() {
  TEST("Metrics: latency is running average");
  ValidationMetricsTracker tracker;
  tracker.recordValidated(ValidationStatus::Valid, 100.0);
  tracker.recordValidated(ValidationStatus::Valid, 200.0);
  auto m = tracker.snapshot();
  ASSERT_NEAR(m.validation_latency_us, 150.0, 1.0);
  PASS();
}

static void test_metrics_reset() {
  TEST("Metrics: reset clears all");
  ValidationMetricsTracker tracker;
  tracker.recordValidated(ValidationStatus::Valid, 10.0);
  tracker.recordSource("fs", ValidationStatus::Invalid);
  tracker.reset();
  auto m = tracker.snapshot();
  ASSERT_EQ(m.events_validated, 0u);
  PASS();
}

// ==================== Source Registry Tests ====================

static void test_source_registry() {
  TEST("SourceRegistry: register and contains");
  SourceRegistry sr;
  ASSERT_FALSE(sr.contains("fs"));
  sr.registerSource("fs");
  ASSERT_TRUE(sr.contains("fs"));
  ASSERT_FALSE(sr.contains("net"));
  ASSERT_EQ(sr.size(), 1u);
  PASS();
}

// ==================== EventSchema Tests ====================

static void test_schema_field_lookup() {
  TEST("EventSchema: field lookup works");
  EventSchema schema;
  schema.name = "test";
  schema.version = 1;
  schema.type = {"filesystem", "modified"};
  schema.required.push_back({"path", FieldType::String, true});
  schema.optional.push_back({"size", FieldType::Unsigned, false});

  ASSERT_TRUE(schema.isRequired("path"));
  ASSERT_FALSE(schema.isRequired("size"));
  ASSERT_TRUE(schema.isOptional("size"));
  ASSERT_TRUE(schema.hasField("path"));
  ASSERT_TRUE(schema.hasField("size"));
  ASSERT_FALSE(schema.hasField("hash"));
  ASSERT_NE(schema.findField("path"), nullptr);
  ASSERT_EQ(schema.findField("hash"), nullptr);
  PASS();
}

// ==================== Performance Tests ====================

static void test_perf_structural_100k() {
  TEST("Perf: StructuralValidator 100K events");
  StructuralValidator sv;
  ValidationContext ctx;
  auto event = makeValidEvent();

  auto start = std::chrono::steady_clock::now();
  for (int i = 0; i < 100000; ++i) {
    sv.validate(event, ctx);
  }
  auto end = std::chrono::steady_clock::now();
  double ms = std::chrono::duration<double, std::milli>(end - start).count();
  printf("(%.0f ms, %.0fK/s) ", ms, 100000.0 / ms);
  PASS();
}

static void test_perf_pipeline_100k() {
  TEST("Perf: Pipeline 100K valid events");
  EventValidationPipeline pipeline;
  auto event = makeValidEvent();

  auto start = std::chrono::steady_clock::now();
  for (int i = 0; i < 100000; ++i) {
    pipeline.validate(event);
  }
  auto end = std::chrono::steady_clock::now();
  double ms = std::chrono::duration<double, std::milli>(end - start).count();
  printf("(%.0f ms, %.0fK/s) ", ms, 100000.0 / ms);
  PASS();
}

static void test_perf_integrity_10k() {
  TEST("Perf: IntegrityValidator 10K events");
  IntegrityValidator iv;
  ValidationContext ctx;
  ctx.policy.verify_integrity = true;
  auto event = makeValidEvent();

  auto start = std::chrono::steady_clock::now();
  for (int i = 0; i < 10000; ++i) {
    iv.validate(event, ctx);
  }
  auto end = std::chrono::steady_clock::now();
  double ms = std::chrono::duration<double, std::milli>(end - start).count();
  printf("(%.0f ms, %.0fK/s) ", ms, 10000.0 / ms);
  PASS();
}

static void test_perf_quarantine_100k_aggregate() {
  TEST("Perf: Quarantine aggregation 100K same failures");
  QuarantineManager qm;
  auto event = makeValidEvent();
  ValidationResult vr;
  vr.status = ValidationStatus::Invalid;
  vr.addIssue(ValidationIssueCode::MissingRequiredField, ValidationSeverity::Error, "path", "x");

  auto start = std::chrono::steady_clock::now();
  for (int i = 0; i < 100000; ++i) {
    qm.quarantine(event, vr, currentSystemTimeMs());
  }
  auto end = std::chrono::steady_clock::now();
  double ms = std::chrono::duration<double, std::milli>(end - start).count();
  printf("(%.0f ms, %zu aggregates) ", ms, qm.aggregateCount());
  PASS();
}

static void test_perf_quarantine_1m_no_noise() {
  TEST("Perf: 1M invalid events -> no 1M log lines (aggregation)");
  QuarantineConfig cfg;
  cfg.max_events = 1000;
  QuarantineManager qm(cfg);
  auto event = makeValidEvent();
  ValidationResult vr;
  vr.status = ValidationStatus::Invalid;
  vr.addIssue(ValidationIssueCode::MissingRequiredField, ValidationSeverity::Error, "path", "x");

  auto start = std::chrono::steady_clock::now();
  for (int i = 0; i < 1000000; ++i) {
    qm.quarantine(event, vr, currentSystemTimeMs());
  }
  auto end = std::chrono::steady_clock::now();
  double ms = std::chrono::duration<double, std::milli>(end - start).count();
  ASSERT_EQ(qm.size(), 1000u);
  ASSERT_EQ(qm.totalQuarantined(), 1000000u);
  auto aggs = qm.aggregates();
  ASSERT_EQ(aggs.size(), 1u);
  ASSERT_EQ(aggs[0].count, 1000000u);
  printf("(%.0f ms, %zu stored, 1 aggregate) ", ms, qm.size());
  PASS();
}

// ==================== Main ====================

#ifndef MONIX_KERNEL_BUILD
int main() {
  printf("=== MONIX Validation Tests ===\n\n");

  printf("[Type Names]\n");
  test_status_names();
  test_severity_names();
  test_issue_code_names();
  test_profile_names();

  printf("\n[Policy]\n");
  test_policy_presets();

  printf("\n[ValidationResult]\n");
  test_result_valid();
  test_result_merge_worsens();
  test_result_merge_invalid();
  test_result_add_issue();

  printf("\n[Structural Validator]\n");
  test_structural_valid_event();
  test_structural_missing_id();
  test_structural_missing_type();
  test_structural_missing_timestamp();
  test_structural_missing_source();
  test_structural_missing_provenance();
  test_structural_multiple_fatal();

  printf("\n[Schema Validator]\n");
  test_schema_no_registry_passes();
  test_schema_unknown_reject();
  test_schema_unknown_warn();
  test_schema_found_passes();
  test_schema_unknown_fields_warn();

  printf("\n[EventSchema]\n");
  test_schema_field_lookup();

  printf("\n[Source Registry]\n");
  test_source_registry();

  printf("\n[Semantic Validator]\n");
  test_semantic_valid_event();
  test_semantic_future_timestamp();
  test_semantic_clock_skew();
  test_semantic_duplicate_id();
  test_semantic_user_actor_no_username();
  test_semantic_file_empty_name();
  test_semantic_action_type_conflict();
  test_semantic_clear_seen_ids();

  printf("\n[Integrity Validator]\n");
  test_integrity_canonicalize();
  test_integrity_hash_deterministic();
  test_integrity_hash_differs_on_modify();
  test_integrity_disabled_passes();
  test_integrity_enabled_computes();

  printf("\n[Pipeline]\n");
  test_pipeline_valid_event();
  test_pipeline_missing_id_fails();
  test_pipeline_to_report();
  test_pipeline_with_schema();

  printf("\n[Quarantine]\n");
  test_quarantine_basic();
  test_quarantine_limits();
  test_quarantine_aggregation();
  test_quarantine_purge_expired();
  test_quarantine_clear();

  printf("\n[Metrics]\n");
  test_metrics_basic();
  test_metrics_source_tracking();
  test_metrics_latency_average();
  test_metrics_reset();

  printf("\n[Performance]\n");
  test_perf_structural_100k();
  test_perf_pipeline_100k();
  test_perf_integrity_10k();
  test_perf_quarantine_100k_aggregate();
  test_perf_quarantine_1m_no_noise();

  printf("\n=== Results: %d passed, %d failed ===\n", gPassed, gFailed);
  return gFailed > 0 ? 1 : 0;
}
#endif

#ifdef MONIX_KERNEL_BUILD
int GetFailedCount_ValidationTests() { return gFailed; }

struct KTestEntry {
  const char* display_name;
  void (*func)();
};

static const KTestEntry s_ktests[] = {
  {"ValidationStatus: names", test_status_names},
  {"ValidationSeverity: names", test_severity_names},
  {"ValidationIssueCode: names", test_issue_code_names},
  {"ValidationProfile: names", test_profile_names},
  {"ValidationPolicy: presets", test_policy_presets},
  {"ValidationResult: valid", test_result_valid},
  {"ValidationResult: merge worsens", test_result_merge_worsens},
  {"ValidationResult: merge invalid", test_result_merge_invalid},
  {"ValidationResult: add issue", test_result_add_issue},
  {"Structural: valid event", test_structural_valid_event},
  {"Structural: missing id", test_structural_missing_id},
  {"Structural: missing type", test_structural_missing_type},
  {"Structural: missing timestamp", test_structural_missing_timestamp},
  {"Structural: missing source", test_structural_missing_source},
  {"Structural: missing provenance", test_structural_missing_provenance},
  {"Structural: multiple fatal", test_structural_multiple_fatal},
  {"Schema: no registry passes", test_schema_no_registry_passes},
  {"Schema: unknown reject", test_schema_unknown_reject},
  {"Schema: unknown warn", test_schema_unknown_warn},
  {"Schema: found passes", test_schema_found_passes},
  {"Schema: unknown fields warn", test_schema_unknown_fields_warn},
  {"Semantic: valid event", test_semantic_valid_event},
  {"Semantic: future timestamp", test_semantic_future_timestamp},
  {"Semantic: clock skew", test_semantic_clock_skew},
  {"Semantic: duplicate id", test_semantic_duplicate_id},
  {"Semantic: user actor no username", test_semantic_user_actor_no_username},
  {"Semantic: file empty name", test_semantic_file_empty_name},
  {"Semantic: action type conflict", test_semantic_action_type_conflict},
  {"Semantic: clear seen ids", test_semantic_clear_seen_ids},
  {"Integrity: canonicalize", test_integrity_canonicalize},
  {"Integrity: hash deterministic", test_integrity_hash_deterministic},
  {"Integrity: hash differs on modify", test_integrity_hash_differs_on_modify},
  {"Integrity: disabled passes", test_integrity_disabled_passes},
  {"Integrity: enabled computes", test_integrity_enabled_computes},
  {"Pipeline: valid event", test_pipeline_valid_event},
  {"Pipeline: missing id fails", test_pipeline_missing_id_fails},
  {"Pipeline: to report", test_pipeline_to_report},
  {"Pipeline: with schema", test_pipeline_with_schema},
  {"Quarantine: basic", test_quarantine_basic},
  {"Quarantine: limits", test_quarantine_limits},
  {"Quarantine: aggregation", test_quarantine_aggregation},
  {"Quarantine: purge expired", test_quarantine_purge_expired},
  {"Quarantine: clear", test_quarantine_clear},
  {"Metrics: basic", test_metrics_basic},
  {"Metrics: source tracking", test_metrics_source_tracking},
  {"Metrics: latency average", test_metrics_latency_average},
  {"Metrics: reset", test_metrics_reset},
  {"SourceRegistry: register", test_source_registry},
  {"EventSchema: field lookup", test_schema_field_lookup},
  {"Perf: structural 100k", test_perf_structural_100k},
  {"Perf: pipeline 100k", test_perf_pipeline_100k},
  {"Perf: integrity 10k", test_perf_integrity_10k},
  {"Perf: quarantine 100k", test_perf_quarantine_100k_aggregate},
  {"Perf: quarantine 1m", test_perf_quarantine_1m_no_noise},
};

const KTestEntry* GetKTests_Validation() { return s_ktests; }
std::size_t GetKTestCount_Validation() { return sizeof(s_ktests) / sizeof(s_ktests[0]); }
#endif
