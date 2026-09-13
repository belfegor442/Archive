#include <cassert>
#include <chrono>
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

#include "CameraAdapter.hpp"
#include "DeviceTrust.hpp"
#include "HashIntegrity.hpp"
#include "ForensicMode.hpp"

using namespace monix::collectors::camera;
using namespace monix::collectors::trust;
using namespace monix::collectors::forensic;

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

// ===== Camera Tests =====

static bool test_cam_event_names() {
  ASSERT_EQ(std::string(CameraEventKindName(CameraEventKind::Connected)), "Connected");
  ASSERT_EQ(std::string(CameraEventKindName(CameraEventKind::StreamUnavailable)), "StreamUnavailable");
  ASSERT_EQ(std::string(CameraEventKindName(CameraEventKind::RecordingStateChanged)), "RecordingStateChanged");
  return true;
}

static bool test_cam_event_actions() {
  ASSERT_EQ(CameraEventKindAction(CameraEventKind::Connected), "external.camera.connected");
  ASSERT_EQ(CameraEventKindAction(CameraEventKind::StreamUnavailable), "external.camera.stream_unavailable");
  ASSERT_EQ(CameraEventKindAction(CameraEventKind::RecordingStateChanged), "external.camera.recording_state_changed");
  return true;
}

static bool test_cam_state_names() {
  ASSERT_EQ(std::string(CameraStateName(CameraState::Connected)), "Connected");
  ASSERT_EQ(std::string(StreamStateName(StreamState::Available)), "Available");
  ASSERT_EQ(std::string(RecordingStateName(RecordingState::Recording)), "Recording");
  return true;
}

static bool test_cam_lifecycle() {
  CameraAdapter adapter;
  adapter.setCallback([](const CameraEvent&) {});

  adapter.registerCamera("CAM1", "Front Door", "onvif", "192.168.1.100");
  ASSERT_EQ(adapter.cameraCount(), static_cast<std::size_t>(1));

  adapter.cameraConnected("CAM1");
  ASSERT_TRUE(adapter.getCamera("CAM1").isConnected());
  ASSERT_EQ(adapter.connectedCount(), static_cast<std::size_t>(1));

  adapter.cameraDisconnected("CAM1");
  ASSERT_FALSE(adapter.getCamera("CAM1").isConnected());
  return true;
}

static bool test_cam_stream() {
  CameraAdapter adapter;
  adapter.setCallback([](const CameraEvent&) {});
  adapter.registerCamera("CAM1");

  adapter.cameraConnected("CAM1");
  adapter.streamUnavailable("CAM1", "network issue");
  ASSERT_EQ(adapter.getCamera("CAM1").stream, StreamState::Unavailable);

  adapter.streamAvailable("CAM1");
  ASSERT_EQ(adapter.getCamera("CAM1").stream, StreamState::Available);
  return true;
}

static bool test_cam_recording() {
  CameraAdapter adapter;
  adapter.setCallback([](const CameraEvent&) {});
  adapter.registerCamera("CAM1");

  adapter.cameraConnected("CAM1");
  adapter.recordingStateChanged("CAM1", RecordingState::Recording);
  ASSERT_TRUE(adapter.getCamera("CAM1").isRecording());
  ASSERT_EQ(adapter.recordingCount(), static_cast<std::size_t>(1));

  adapter.recordingStateChanged("CAM1", RecordingState::Stopped);
  ASSERT_FALSE(adapter.getCamera("CAM1").isRecording());
  return true;
}

static bool test_cam_metadata_only() {
  CameraAdapter adapter;
  std::string action;
  adapter.setCallback([&](const CameraEvent& e) { action = e.action(); });

  adapter.registerCamera("CAM1");
  adapter.streamUnavailable("CAM1", "network");
  ASSERT_EQ(action, "external.camera.stream_unavailable");
  ASSERT_FALSE(action.find("video") != std::string::npos);
  return true;
}

static bool test_cam_events_emitted() {
  CameraAdapter adapter;
  adapter.setCallback([](const CameraEvent&) {});
  adapter.registerCamera("CAM1");
  adapter.cameraConnected("CAM1");
  adapter.cameraDisconnected("CAM1");
  ASSERT_TRUE(adapter.eventsEmitted() >= static_cast<std::size_t>(2));
  return true;
}

// ===== Trust Tests =====

static bool test_trust_level_names() {
  ASSERT_EQ(std::string(TrustLevelName(TrustLevel::Unknown)), "Unknown");
  ASSERT_EQ(std::string(TrustLevelName(TrustLevel::Local)), "Local");
  ASSERT_EQ(std::string(TrustLevelName(TrustLevel::Trusted)), "Trusted");
  ASSERT_EQ(std::string(TrustLevelName(TrustLevel::Verified)), "Verified");
  return true;
}

static bool test_trust_from_name() {
  ASSERT_EQ(TrustLevelFromName("Unknown"), TrustLevel::Unknown);
  ASSERT_EQ(TrustLevelFromName("Local"), TrustLevel::Local);
  ASSERT_EQ(TrustLevelFromName("Trusted"), TrustLevel::Trusted);
  ASSERT_EQ(TrustLevelFromName("Verified"), TrustLevel::Verified);
  ASSERT_EQ(TrustLevelFromName("invalid"), TrustLevel::Unknown);
  return true;
}

static bool test_trust_register() {
  DeviceTrustManager mgr;
  mgr.registerDevice("CAM1", TrustLevel::Local, "onvif", "admin");
  ASSERT_TRUE(mgr.hasDevice("CAM1"));
  ASSERT_EQ(mgr.getLevel("CAM1"), TrustLevel::Local);
  return true;
}

static bool test_trust_upgrade() {
  DeviceTrustManager mgr;
  mgr.setCallback([](const TrustChangeEvent&) {});
  mgr.registerDevice("DEV1", TrustLevel::Unknown);

  mgr.setTrustLevel("DEV1", TrustLevel::Local, "detected on LAN");
  ASSERT_EQ(mgr.getLevel("DEV1"), TrustLevel::Local);

  mgr.setTrustLevel("DEV1", TrustLevel::Trusted, "cert verified");
  ASSERT_TRUE(mgr.isTrusted("DEV1"));

  mgr.setTrustLevel("DEV1", TrustLevel::Verified, "full audit");
  ASSERT_TRUE(mgr.isVerified("DEV1"));
  return true;
}

static bool test_trust_query() {
  DeviceTrustManager mgr;
  mgr.registerDevice("A", TrustLevel::Trusted);
  mgr.registerDevice("B", TrustLevel::Local);
  mgr.registerDevice("C", TrustLevel::Trusted);
  mgr.registerDevice("D", TrustLevel::Verified);

  auto trusted = mgr.byLevel(TrustLevel::Trusted);
  ASSERT_EQ(trusted.size(), static_cast<std::size_t>(2));
  return true;
}

static bool test_trust_not_security() {
  DeviceTrustManager mgr;
  mgr.registerDevice("DEV1", TrustLevel::Verified);
  ASSERT_TRUE(mgr.isVerified("DEV1"));
  DeviceTrust trust = mgr.getDevice("DEV1");
  ASSERT_TRUE(trust.summary().find("Verified") != std::string::npos);
  return true;
}

static bool test_trust_change_event() {
  DeviceTrustManager mgr;
  TrustChangeEvent last_event;
  mgr.setCallback([&](const TrustChangeEvent& e) { last_event = e; });

  mgr.registerDevice("X", TrustLevel::Unknown);
  mgr.setTrustLevel("X", TrustLevel::Trusted, "manual");

  ASSERT_EQ(last_event.old_level, TrustLevel::Unknown);
  ASSERT_EQ(last_event.new_level, TrustLevel::Trusted);
  return true;
}

// ===== Hash Integrity Tests =====

static bool test_hash_mode_names() {
  ASSERT_EQ(std::string(HashModeName(HashMode::Disabled)), "Disabled");
  ASSERT_EQ(std::string(HashModeName(HashMode::OnDemand)), "OnDemand");
  ASSERT_EQ(std::string(HashModeName(HashMode::Forensic)), "Forensic");
  ASSERT_EQ(std::string(HashModeName(HashMode::Always)), "Always");
  return true;
}

static bool test_hash_event_actions() {
  ASSERT_EQ(HashEventKindAction(HashEventKind::Calculated), "file.hash.calculated");
  ASSERT_EQ(HashEventKindAction(HashEventKind::IntegrityChanged), "file.integrity.changed");
  return true;
}

static bool test_hash_disabled() {
  HashConfig cfg;
  cfg.mode = HashMode::Disabled;
  HashIntegrity hi(cfg);
  ASSERT_TRUE(hi.isDisabled());
  ASSERT_FALSE(hi.shouldHash("test.txt", 1024));
  return true;
}

static bool test_hash_always() {
  HashConfig cfg;
  cfg.mode = HashMode::Always;
  cfg.max_file_size_mb = 10;
  HashIntegrity hi(cfg);
  ASSERT_TRUE(hi.shouldHash("test.txt", 1024));
  ASSERT_FALSE(hi.shouldHash("big.bin", 20 * 1024 * 1024));
  return true;
}

static bool test_hash_small_file() {
  HashConfig cfg;
  cfg.mode = HashMode::Always;
  cfg.log_calculated_events = true;
  HashIntegrity hi(cfg);
  hi.setCallback([](const HashEvent&) {});

  std::vector<std::uint8_t> data = {0x48, 0x65, 0x6C, 0x6C, 0x6F};
  auto event = hi.computeHash("test.txt", data);
  ASSERT_TRUE(event.isValid());
  ASSERT_FALSE(event.hash_value.empty());
  ASSERT_EQ(event.hash_algorithm, "SHA-256");
  return true;
}

static bool test_hash_stable() {
  HashConfig cfg;
  cfg.mode = HashMode::Always;
  cfg.log_calculated_events = true;
  HashIntegrity hi(cfg);
  hi.setCallback([](const HashEvent&) {});

  std::vector<std::uint8_t> data = {0x48, 0x65, 0x6C, 0x6C, 0x6F};
  auto e1 = hi.computeHash("test.txt", data);
  auto e2 = hi.computeHash("test.txt", data);
  ASSERT_EQ(e1.hash_value, e2.hash_value);
  return true;
}

static bool test_hash_changed() {
  HashConfig cfg;
  cfg.mode = HashMode::Always;
  cfg.log_changed_events = true;
  HashIntegrity hi(cfg);
  bool changed_event = false;
  hi.setCallback([&](const HashEvent& e) {
    if (e.kind == HashEventKind::IntegrityChanged) changed_event = true;
  });

  std::vector<std::uint8_t> data1 = {0x48, 0x65, 0x6C};
  hi.computeHash("test.txt", data1);
  hi.storeBaseline("test.txt", hi.computeHash("test.txt", data1).hash_value);

  std::vector<std::uint8_t> data2 = {0x48, 0x65, 0x6C, 0x6C, 0x6F};
  auto event = hi.computeHash("test.txt", data2);
  ASSERT_TRUE(event.integrity_changed);
  ASSERT_TRUE(changed_event);
  return true;
}

static bool test_hash_permission() {
  HashConfig cfg;
  cfg.mode = HashMode::Disabled;
  HashIntegrity hi(cfg);
  ASSERT_FALSE(hi.shouldHash("any.txt", 100));
  return true;
}

static bool test_hash_large_file_chunked() {
  HashConfig cfg;
  cfg.mode = HashMode::Always;
  HashIntegrity hi(cfg);
  hi.setCallback([](const HashEvent&) {});

  std::vector<std::vector<std::uint8_t>> chunks;
  for (int i = 0; i < 10; i++) {
    chunks.push_back({0x48, 0x65, 0x6C, 0x6C, 0x6F});
  }
  auto event = hi.computeHashChunked("large.bin", chunks);
  ASSERT_TRUE(event.isValid());
  ASSERT_EQ(event.file_size, static_cast<std::size_t>(50));
  return true;
}

// ===== Forensic Mode Tests =====

static bool test_fm_level_names() {
  ASSERT_EQ(std::string(ForensicLevelName(ForensicLevel::Disabled)), "Disabled");
  ASSERT_EQ(std::string(ForensicLevelName(ForensicLevel::Minimal)), "Minimal");
  ASSERT_EQ(std::string(ForensicLevelName(ForensicLevel::Standard)), "Standard");
  ASSERT_EQ(std::string(ForensicLevelName(ForensicLevel::Enhanced)), "Enhanced");
  ASSERT_EQ(std::string(ForensicLevelName(ForensicLevel::Full)), "Full");
  return true;
}

static bool test_fm_default_config() {
  auto cfg = ForensicConfig::defaultForLevel(ForensicLevel::Standard);
  ASSERT_TRUE(cfg.filesystem_events);
  ASSERT_TRUE(cfg.process_metadata);
  ASSERT_TRUE(cfg.hash_files);
  ASSERT_FALSE(cfg.additional_snapshots);
  return true;
}

static bool test_fm_activate() {
  ForensicMode fm;
  ASSERT_TRUE(fm.activate(ForensicLevel::Standard));
  ASSERT_TRUE(fm.isActive());
  ASSERT_EQ(fm.currentLevel(), ForensicLevel::Standard);
  return true;
}

static bool test_fm_deactivate() {
  ForensicMode fm;
  fm.activate(ForensicLevel::Full);
  ASSERT_TRUE(fm.deactivate());
  ASSERT_FALSE(fm.isActive());
  ASSERT_TRUE(fm.isDisabled());
  return true;
}

static bool test_fm_should_collect() {
  ForensicMode fm;
  fm.activate(ForensicLevel::Standard);
  ASSERT_TRUE(fm.shouldCollectFilesystem());
  ASSERT_TRUE(fm.shouldCollectProcessMetadata());
  ASSERT_TRUE(fm.shouldHash());
  ASSERT_FALSE(fm.shouldTakeSnapshots());
  ASSERT_FALSE(fm.shouldExtendProvenance());
  return true;
}

static bool test_fm_full_level() {
  ForensicMode fm;
  fm.activate(ForensicLevel::Full);
  ASSERT_TRUE(fm.shouldCollectFilesystem());
  ASSERT_TRUE(fm.shouldCollectProcessMetadata());
  ASSERT_TRUE(fm.shouldHash());
  ASSERT_TRUE(fm.shouldTakeSnapshots());
  ASSERT_TRUE(fm.shouldExtendProvenance());
  ASSERT_TRUE(fm.shouldCollectDiagnostics());
  return true;
}

static bool test_fm_memory_limit() {
  auto cfg = ForensicConfig::defaultForLevel(ForensicLevel::Standard);
  ForensicMode fm(cfg);
  fm.activate(ForensicLevel::Standard);
  ASSERT_TRUE(fm.withinMemoryLimit(100 * 1024 * 1024));
  ASSERT_FALSE(fm.withinMemoryLimit(500 * 1024 * 1024));
  return true;
}

static bool test_fm_queue_limit() {
  auto cfg = ForensicConfig::defaultForLevel(ForensicLevel::Standard);
  ForensicMode fm(cfg);
  fm.activate(ForensicLevel::Standard);
  ASSERT_TRUE(fm.withinQueueLimit(5000));
  ASSERT_FALSE(fm.withinQueueLimit(50000));
  return true;
}

static bool test_fm_rate_limit() {
  auto cfg = ForensicConfig::defaultForLevel(ForensicLevel::Standard);
  ForensicMode fm(cfg);
  fm.activate(ForensicLevel::Standard);
  ASSERT_TRUE(fm.withinRateLimit(500));
  ASSERT_FALSE(fm.withinRateLimit(5000));
  return true;
}

static bool test_fm_state() {
  ForensicMode fm;
  fm.activate(ForensicLevel::Minimal);
  fm.recordEvent();
  fm.recordEvent();
  fm.recordSnapshot();
  fm.recordHash();

  auto state = fm.getState();
  ASSERT_TRUE(state.isActive());
  ASSERT_EQ(state.events_generated, static_cast<std::size_t>(2));
  ASSERT_EQ(state.snapshots_taken, static_cast<std::size_t>(1));
  ASSERT_EQ(state.hashes_computed, static_cast<std::size_t>(1));
  return true;
}

static bool test_fm_double_activate() {
  ForensicMode fm;
  ASSERT_TRUE(fm.activate(ForensicLevel::Minimal));
  ASSERT_FALSE(fm.activate(ForensicLevel::Full));
  ASSERT_EQ(fm.currentLevel(), ForensicLevel::Minimal);
  return true;
}

#ifndef MONIX_KERNEL_BUILD
int main() {
  std::cerr << "=== MONIX Phases 30-33 Tests ===\n\n";

  RUN_TEST(cam_event_names);
  RUN_TEST(cam_event_actions);
  RUN_TEST(cam_state_names);
  RUN_TEST(cam_lifecycle);
  RUN_TEST(cam_stream);
  RUN_TEST(cam_recording);
  RUN_TEST(cam_metadata_only);
  RUN_TEST(cam_events_emitted);
  RUN_TEST(trust_level_names);
  RUN_TEST(trust_from_name);
  RUN_TEST(trust_register);
  RUN_TEST(trust_upgrade);
  RUN_TEST(trust_query);
  RUN_TEST(trust_not_security);
  RUN_TEST(trust_change_event);
  RUN_TEST(hash_mode_names);
  RUN_TEST(hash_event_actions);
  RUN_TEST(hash_disabled);
  RUN_TEST(hash_always);
  RUN_TEST(hash_small_file);
  RUN_TEST(hash_stable);
  RUN_TEST(hash_changed);
  RUN_TEST(hash_permission);
  RUN_TEST(hash_large_file_chunked);
  RUN_TEST(fm_level_names);
  RUN_TEST(fm_default_config);
  RUN_TEST(fm_activate);
  RUN_TEST(fm_deactivate);
  RUN_TEST(fm_should_collect);
  RUN_TEST(fm_full_level);
  RUN_TEST(fm_memory_limit);
  RUN_TEST(fm_queue_limit);
  RUN_TEST(fm_rate_limit);
  RUN_TEST(fm_state);
  RUN_TEST(fm_double_activate);

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
  {"Camera event names", test_cam_event_names},
  {"Camera event actions", test_cam_event_actions},
  {"Camera state names", test_cam_state_names},
  {"Camera lifecycle", test_cam_lifecycle},
  {"Camera stream", test_cam_stream},
  {"Camera recording", test_cam_recording},
  {"Camera metadata only", test_cam_metadata_only},
  {"Camera events emitted", test_cam_events_emitted},
  {"Trust level names", test_trust_level_names},
  {"Trust from name", test_trust_from_name},
  {"Trust register", test_trust_register},
  {"Trust upgrade", test_trust_upgrade},
  {"Trust query", test_trust_query},
  {"Trust not security", test_trust_not_security},
  {"Trust change event", test_trust_change_event},
  {"Hash mode names", test_hash_mode_names},
  {"Hash event actions", test_hash_event_actions},
  {"Hash disabled", test_hash_disabled},
  {"Hash always", test_hash_always},
  {"Hash small file", test_hash_small_file},
  {"Hash stable", test_hash_stable},
  {"Hash changed", test_hash_changed},
  {"Hash permission", test_hash_permission},
  {"Hash large file chunked", test_hash_large_file_chunked},
  {"Forensics level names", test_fm_level_names},
  {"Forensics default config", test_fm_default_config},
  {"Forensics activate", test_fm_activate},
  {"Forensics deactivate", test_fm_deactivate},
  {"Forensics should collect", test_fm_should_collect},
  {"Forensics full level", test_fm_full_level},
  {"Forensics memory limit", test_fm_memory_limit},
  {"Forensics queue limit", test_fm_queue_limit},
  {"Forensics rate limit", test_fm_rate_limit},
  {"Forensics state", test_fm_state},
  {"Forensics double activate", test_fm_double_activate},
};

const KBoolTestEntry* GetKBoolTests_Phases30_33() { return s_kbooltests; }
std::size_t GetKBoolTestCount_Phases30_33() { return sizeof(s_kbooltests) / sizeof(s_kbooltests[0]); }
#endif
