#include "../../../core/collectors/filesystem/FilesystemTypes.hpp"
#include "../../../core/collectors/filesystem/FilesystemDeduplicator.hpp"
#include "../../../core/collectors/filesystem/FilesystemCoalescer.hpp"
#include "../../../core/collectors/filesystem/FilesystemHasher.hpp"
#include "../../../core/collectors/filesystem/FilesystemCollector.hpp"
#include "../../../core/events/Event.hpp"

#include <cassert>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <atomic>

using namespace monix::collectors;
using namespace monix::collectors::fs;
using namespace monix::events;

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

static std::int64_t nowMs() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::steady_clock::now().time_since_epoch()).count();
}

static std::filesystem::path testDir() {
  auto p = std::filesystem::temp_directory_path() / "monix_fs_test";
  std::error_code ec;
  std::filesystem::create_directories(p, ec);
  return p;
}

static void cleanupDir(const std::filesystem::path& p) {
  std::error_code ec;
  std::filesystem::remove_all(p, ec);
}

static void writeFile(const std::filesystem::path& path, const std::string& content) {
  std::ofstream f(path, std::ios::binary);
  f.write(content.data(), static_cast<std::streamsize>(content.size()));
  f.close();
}

// ==================== Enum Tests ====================

static void test_event_kind_names() {
  TEST("FilesystemEventKind: all names");
  ASSERT_EQ(std::string(FilesystemEventKindName(FilesystemEventKind::FileCreated)), "FileCreated");
  ASSERT_EQ(std::string(FilesystemEventKindName(FilesystemEventKind::FileModified)), "FileModified");
  ASSERT_EQ(std::string(FilesystemEventKindName(FilesystemEventKind::FileDeleted)), "FileDeleted");
  ASSERT_EQ(std::string(FilesystemEventKindName(FilesystemEventKind::FileMoved)), "FileMoved");
  ASSERT_EQ(std::string(FilesystemEventKindName(FilesystemEventKind::FileRenamed)), "FileRenamed");
  ASSERT_EQ(std::string(FilesystemEventKindName(FilesystemEventKind::DirectoryCreated)), "DirectoryCreated");
  ASSERT_EQ(std::string(FilesystemEventKindName(FilesystemEventKind::DirectoryDeleted)), "DirectoryDeleted");
  ASSERT_EQ(std::string(FilesystemEventKindName(FilesystemEventKind::DirectoryMoved)), "DirectoryMoved");
  ASSERT_EQ(std::string(FilesystemEventKindName(FilesystemEventKind::DirectoryRenamed)), "DirectoryRenamed");
  PASS();
}

static void test_hash_mode_names() {
  TEST("FileHashMode: all names");
  ASSERT_EQ(std::string(FileHashModeName(FileHashMode::Disabled)), "Disabled");
  ASSERT_EQ(std::string(FileHashModeName(FileHashMode::OnDemand)), "OnDemand");
  ASSERT_EQ(std::string(FileHashModeName(FileHashMode::Suspicious)), "Suspicious");
  ASSERT_EQ(std::string(FileHashModeName(FileHashMode::Forensic)), "Forensic");
  ASSERT_EQ(std::string(FileHashModeName(FileHashMode::Always)), "Always");
  PASS();
}

static void test_watcher_mode_names() {
  TEST("WatcherMode: all names");
  ASSERT_EQ(std::string(WatcherModeName(WatcherMode::Native)), "Native");
  ASSERT_EQ(std::string(WatcherModeName(WatcherMode::Polling)), "Polling");
  ASSERT_EQ(std::string(WatcherModeName(WatcherMode::Hybrid)), "Hybrid");
  PASS();
}

static void test_hash_mode_from_string() {
  TEST("FileHashMode: fromString roundtrip");
  ASSERT_EQ(FileHashModeFromString("Disabled"), FileHashMode::Disabled);
  ASSERT_EQ(FileHashModeFromString("Always"), FileHashMode::Always);
  ASSERT_EQ(FileHashModeFromString("invalid"), FileHashMode::Disabled);
  PASS();
}

// ==================== Event Type Tests ====================

static void test_event_type_file() {
  TEST("FilesystemEvent: file event_type");
  FilesystemEvent fe;
  fe.kind = FilesystemEventKind::FileCreated;
  ASSERT_EQ(fe.event_type(), "filesystem.file.created");
  ASSERT_TRUE(fe.is_file());
  ASSERT_FALSE(fe.is_directory());
  PASS();
}

static void test_event_type_directory() {
  TEST("FilesystemEvent: directory event_type");
  FilesystemEvent fe;
  fe.kind = FilesystemEventKind::DirectoryCreated;
  ASSERT_EQ(fe.event_type(), "filesystem.directory.created");
  ASSERT_FALSE(fe.is_file());
  ASSERT_TRUE(fe.is_directory());
  PASS();
}

// ==================== Deduplicator Tests ====================

static void test_dedup_first_event_passes() {
  TEST("Dedup: first event passes");
  FilesystemDeduplicator dedup(200);
  FilesystemEvent fe;
  fe.kind = FilesystemEventKind::FileModified;
  fe.path = "/tmp/test.txt";
  ASSERT_FALSE(dedup.isDuplicate(fe, nowMs()));
  PASS();
}

static void test_dedup_same_event_duplicate() {
  TEST("Dedup: same event within window is duplicate");
  FilesystemDeduplicator dedup(500);
  FilesystemEvent fe;
  fe.kind = FilesystemEventKind::FileModified;
  fe.path = "/tmp/test.txt";
  auto t = nowMs();
  ASSERT_FALSE(dedup.isDuplicate(fe, t));
  ASSERT_TRUE(dedup.isDuplicate(fe, t + 100));
  PASS();
}

static void test_dedup_different_path_not_duplicate() {
  TEST("Dedup: different path is not duplicate");
  FilesystemDeduplicator dedup(500);
  FilesystemEvent fe1;
  fe1.kind = FilesystemEventKind::FileModified;
  fe1.path = "/tmp/a.txt";
  FilesystemEvent fe2;
  fe2.kind = FilesystemEventKind::FileModified;
  fe2.path = "/tmp/b.txt";
  auto t = nowMs();
  ASSERT_FALSE(dedup.isDuplicate(fe1, t));
  ASSERT_FALSE(dedup.isDuplicate(fe2, t));
  PASS();
}

static void test_dedup_different_kind_not_duplicate() {
  TEST("Dedup: same path different kind is not duplicate");
  FilesystemDeduplicator dedup(500);
  FilesystemEvent fe1;
  fe1.kind = FilesystemEventKind::FileCreated;
  fe1.path = "/tmp/test.txt";
  FilesystemEvent fe2;
  fe2.kind = FilesystemEventKind::FileModified;
  fe2.path = "/tmp/test.txt";
  auto t = nowMs();
  ASSERT_FALSE(dedup.isDuplicate(fe1, t));
  ASSERT_FALSE(dedup.isDuplicate(fe2, t));
  PASS();
}

static void test_dedup_after_window_passes() {
  TEST("Dedup: event after window passes");
  FilesystemDeduplicator dedup(100);
  FilesystemEvent fe;
  fe.kind = FilesystemEventKind::FileModified;
  fe.path = "/tmp/test.txt";
  auto t = nowMs();
  ASSERT_FALSE(dedup.isDuplicate(fe, t));
  ASSERT_TRUE(dedup.isDuplicate(fe, t + 50));
  ASSERT_FALSE(dedup.isDuplicate(fe, t + 200));
  PASS();
}

static void test_dedup_clear() {
  TEST("Dedup: clear resets state");
  FilesystemDeduplicator dedup(500);
  FilesystemEvent fe;
  fe.kind = FilesystemEventKind::FileModified;
  fe.path = "/tmp/test.txt";
  dedup.isDuplicate(fe, nowMs());
  dedup.clear();
  ASSERT_FALSE(dedup.isDuplicate(fe, nowMs()));
  PASS();
}

// ==================== Coalescer Tests ====================

static void test_coalesce_first_event_new() {
  TEST("Coalesce: first event is new");
  FilesystemCoalescer coal(500);
  FilesystemEvent fe;
  fe.kind = FilesystemEventKind::FileModified;
  fe.path = "/tmp/test.txt";
  fe.size = 100;
  ASSERT_TRUE(coal.addEvent(std::move(fe), nowMs()));
  ASSERT_EQ(coal.pendingCount(), 1u);
  PASS();
}

static void test_coalesce_same_event_merges() {
  TEST("Coalesce: same event merges (modification_count++)");
  FilesystemCoalescer coal(500);
  auto t = nowMs();

  FilesystemEvent fe1;
  fe1.kind = FilesystemEventKind::FileModified;
  fe1.path = "/tmp/test.txt";
  fe1.size = 100;
  coal.addEvent(std::move(fe1), t);

  FilesystemEvent fe2;
  fe2.kind = FilesystemEventKind::FileModified;
  fe2.path = "/tmp/test.txt";
  fe2.size = 200;
  ASSERT_FALSE(coal.addEvent(std::move(fe2), t + 50));

  ASSERT_EQ(coal.pendingCount(), 1u);
  auto flushed = coal.flush(t + 600);
  ASSERT_EQ(flushed.size(), 1u);
  ASSERT_EQ(flushed[0].modification_count, 2u);
  ASSERT_EQ(flushed[0].size, 200u);
  ASSERT_EQ(flushed[0].first_seen_ms, t);
  ASSERT_EQ(flushed[0].last_seen_ms, t + 50);
  PASS();
}

static void test_coalesce_different_path_separate() {
  TEST("Coalesce: different paths are separate events");
  FilesystemCoalescer coal(500);
  auto t = nowMs();

  FilesystemEvent fe1;
  fe1.kind = FilesystemEventKind::FileModified;
  fe1.path = "/tmp/a.txt";
  coal.addEvent(std::move(fe1), t);

  FilesystemEvent fe2;
  fe2.kind = FilesystemEventKind::FileModified;
  fe2.path = "/tmp/b.txt";
  coal.addEvent(std::move(fe2), t);

  ASSERT_EQ(coal.pendingCount(), 2u);
  PASS();
}

static void test_coalesce_different_kind_separate() {
  TEST("Coalesce: same path different kind are separate");
  FilesystemCoalescer coal(500);
  auto t = nowMs();

  FilesystemEvent fe1;
  fe1.kind = FilesystemEventKind::FileCreated;
  fe1.path = "/tmp/test.txt";
  coal.addEvent(std::move(fe1), t);

  FilesystemEvent fe2;
  fe2.kind = FilesystemEventKind::FileModified;
  fe2.path = "/tmp/test.txt";
  coal.addEvent(std::move(fe2), t);

  ASSERT_EQ(coal.pendingCount(), 2u);
  PASS();
}

static void test_coalesce_flush_before_window() {
  TEST("Coalesce: flush before window returns nothing");
  FilesystemCoalescer coal(500);
  FilesystemEvent fe;
  fe.kind = FilesystemEventKind::FileModified;
  fe.path = "/tmp/test.txt";
  coal.addEvent(std::move(fe), nowMs());

  auto flushed = coal.flush(nowMs() + 100);
  ASSERT_EQ(flushed.size(), 0u);
  ASSERT_EQ(coal.pendingCount(), 1u);
  PASS();
}

static void test_coalesce_flush_after_window() {
  TEST("Coalesce: flush after window returns events");
  FilesystemCoalescer coal(100);
  FilesystemEvent fe;
  fe.kind = FilesystemEventKind::FileModified;
  fe.path = "/tmp/test.txt";
  auto t = nowMs();
  coal.addEvent(std::move(fe), t);

  auto flushed = coal.flush(t + 200);
  ASSERT_EQ(flushed.size(), 1u);
  ASSERT_EQ(coal.pendingCount(), 0u);
  PASS();
}

static void test_coalesce_hash_propagation() {
  TEST("Coalesce: hash from later event propagates");
  FilesystemCoalescer coal(500);
  auto t = nowMs();

  FilesystemEvent fe1;
  fe1.kind = FilesystemEventKind::FileModified;
  fe1.path = "/tmp/test.txt";
  coal.addEvent(std::move(fe1), t);

  FilesystemEvent fe2;
  fe2.kind = FilesystemEventKind::FileModified;
  fe2.path = "/tmp/test.txt";
  fe2.hash = "abc123";
  fe2.hash_algorithm = "SHA-256";
  coal.addEvent(std::move(fe2), t + 50);

  auto flushed = coal.flush(t + 600);
  ASSERT_EQ(flushed.size(), 1u);
  ASSERT_EQ(flushed[0].hash, "abc123");
  ASSERT_EQ(flushed[0].hash_algorithm, "SHA-256");
  PASS();
}

// ==================== Hasher Tests ====================

static void test_hash_bytes_deterministic() {
  TEST("Hasher: hashBytes is deterministic");
  std::string data = "hello world";
  std::string h1 = FilesystemHasher::hashBytes(data.data(), data.size());
  std::string h2 = FilesystemHasher::hashBytes(data.data(), data.size());
  ASSERT_FALSE(h1.empty());
  ASSERT_EQ(h1, h2);
  PASS();
}

static void test_hash_bytes_different_data() {
  TEST("Hasher: different data produces different hash");
  std::string d1 = "hello";
  std::string d2 = "world";
  std::string h1 = FilesystemHasher::hashBytes(d1.data(), d1.size());
  std::string h2 = FilesystemHasher::hashBytes(d2.data(), d2.size());
  ASSERT_NE(h1, h2);
  PASS();
}

static void test_hash_file() {
  TEST("Hasher: hashFile produces correct SHA-256");
  auto dir = testDir();
  auto path = dir / "hash_test.txt";
  writeFile(path, "test content");

  std::string h = FilesystemHasher::hashFile(path, FileHashMode::Always);
  ASSERT_FALSE(h.empty());
  ASSERT_EQ(h.size(), 64u);

  std::string h2 = FilesystemHasher::hashFile(path, FileHashMode::Always);
  ASSERT_EQ(h, h2);

  cleanupDir(dir);
  PASS();
}

static void test_hash_disabled_returns_empty() {
  TEST("Hasher: Disabled mode returns empty");
  auto dir = testDir();
  auto path = dir / "hash_test.txt";
  writeFile(path, "test");

  std::string h = FilesystemHasher::hashFile(path, FileHashMode::Disabled);
  ASSERT_TRUE(h.empty());

  cleanupDir(dir);
  PASS();
}

// ==================== Collector Tests ====================

static void test_collector_lifecycle() {
  TEST("FilesystemCollector: full lifecycle");
  FilesystemCollector collector;
  ASSERT_EQ(collector.status().lifecycle, CollectorLifecycle::Created);
  ASSERT_TRUE(collector.start(CollectorConfig{}));
  ASSERT_EQ(collector.status().lifecycle, CollectorLifecycle::Running);
  ASSERT_TRUE(collector.stop());
  ASSERT_EQ(collector.status().lifecycle, CollectorLifecycle::Stopped);
  PASS();
}

static void test_collector_info() {
  TEST("FilesystemCollector: info populated");
  FilesystemCollector collector;
  ASSERT_EQ(collector.id(), "filesystem");
  ASSERT_FALSE(collector.info().name.empty());
  ASSERT_FALSE(collector.info().version.empty());
  PASS();
}

static void test_collector_capabilities() {
  TEST("FilesystemCollector: has Realtime|Snapshot|Recovery");
  FilesystemCollector collector;
  auto caps = collector.capabilities();
  ASSERT_TRUE(hasCapability(caps, CollectorCapability::Realtime));
  ASSERT_TRUE(hasCapability(caps, CollectorCapability::Snapshot));
  ASSERT_TRUE(hasCapability(caps, CollectorCapability::Recovery));
  PASS();
}

static void test_collector_start_duplicate() {
  TEST("FilesystemCollector: start while running returns false");
  FilesystemCollector collector;
  collector.start(CollectorConfig{});
  ASSERT_FALSE(collector.start(CollectorConfig{}));
  collector.stop();
  PASS();
}

static void test_collector_stop_without_start() {
  TEST("FilesystemCollector: stop without start returns false");
  FilesystemCollector collector;
  ASSERT_FALSE(collector.stop());
  PASS();
}

static void test_collector_collects_events() {
  TEST("FilesystemCollector: polling collects events");
  auto dir = testDir();
  auto subDir = dir / "watch_test";
  std::error_code ec;
  std::filesystem::create_directories(subDir, ec);

  FilesystemCollector collector;
  fs::FilesystemConfig fsCfg;
  fsCfg.watch_paths.push_back(subDir);
  fsCfg.watcher_mode = WatcherMode::Polling;
  fsCfg.polling_interval_ms = 100;
  fsCfg.coalesce_window_ms = 50;
  collector.setFilesystemConfig(std::move(fsCfg));

  CollectorConfig cfg;
  cfg.set(CollectorConfigField::SamplingIntervalMs, std::int64_t(100));

  std::atomic<int> eventCount{0};
  collector.setEventCallback([&](Event) {
    eventCount++;
  });

  collector.start(cfg);
  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  writeFile(subDir / "test1.txt", "content1");
  std::this_thread::sleep_for(std::chrono::milliseconds(150));

  writeFile(subDir / "test2.txt", "content2");
  std::this_thread::sleep_for(std::chrono::milliseconds(150));

  collector.stop();

  ASSERT_TRUE(eventCount.load() > 0);
  cleanupDir(dir);
  PASS();
}

static void test_collector_delete_detection() {
  TEST("FilesystemCollector: delete detected via polling");
  auto dir = testDir();
  auto subDir = dir / "del_test";
  std::error_code ec;
  std::filesystem::create_directories(subDir, ec);
  writeFile(subDir / "to_delete.txt", "content");

  FilesystemCollector collector;
  fs::FilesystemConfig fsCfg;
  fsCfg.watch_paths.push_back(subDir);
  fsCfg.watcher_mode = WatcherMode::Polling;
  fsCfg.polling_interval_ms = 100;
  fsCfg.coalesce_window_ms = 0;
  collector.setFilesystemConfig(std::move(fsCfg));

  CollectorConfig cfg;
  cfg.set(CollectorConfigField::SamplingIntervalMs, std::int64_t(100));

  std::atomic<int> deleteCount{0};
  collector.setEventCallback([&](Event e) {
    if (e.type.name == "file" && e.type.namespace_name == "filesystem") {
      if (e.payload.has("path")) {
        auto p = e.payload.getString("path");
        if (p.find("to_delete") != std::string::npos &&
            e.action.has_value() && e.action->name == "deleted") {
          deleteCount++;
        }
      }
    }
  });

  collector.start(cfg);
  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  std::filesystem::remove(subDir / "to_delete.txt", ec);
  std::this_thread::sleep_for(std::chrono::milliseconds(150));

  collector.stop();

  ASSERT_TRUE(deleteCount.load() >= 1);
  cleanupDir(dir);
  PASS();
}

static void test_collector_directory_detection() {
  TEST("FilesystemCollector: directory create/delete detected");
  auto dir = testDir();
  auto subDir = dir / "dir_test";
  std::error_code ec;
  std::filesystem::create_directories(subDir, ec);

  FilesystemCollector collector;
  fs::FilesystemConfig fsCfg;
  fsCfg.watch_paths.push_back(subDir);
  fsCfg.watcher_mode = WatcherMode::Polling;
  fsCfg.polling_interval_ms = 100;
  fsCfg.coalesce_window_ms = 0;
  collector.setFilesystemConfig(std::move(fsCfg));

  CollectorConfig cfg;
  cfg.set(CollectorConfigField::SamplingIntervalMs, std::int64_t(100));

  std::atomic<int> dirEvents{0};
  collector.setEventCallback([&](Event e) {
    if (e.type.namespace_name == "filesystem" && e.type.name == "directory") {
      dirEvents++;
    }
  });

  collector.start(cfg);
  std::this_thread::sleep_for(std::chrono::milliseconds(200));

  auto newDir = subDir / "new_subdir";
  std::filesystem::create_directories(newDir, ec);
  std::this_thread::sleep_for(std::chrono::milliseconds(150));

  std::filesystem::remove_all(newDir, ec);
  std::this_thread::sleep_for(std::chrono::milliseconds(150));

  collector.stop();

  ASSERT_TRUE(dirEvents.load() >= 1);
  cleanupDir(dir);
  PASS();
}

// ==================== Path Normalization Tests ====================

static void test_path_normalization() {
  TEST("Path: double slashes normalized");
  std::filesystem::path p("/tmp//test///file.txt");
  auto norm = std::filesystem::weakly_canonical(p);
  ASSERT_TRUE(norm.string().find("//") == std::string::npos);
  PASS();
}

static void test_path_is_absolute() {
  TEST("Path: temp directory is absolute");
  ASSERT_TRUE(std::filesystem::temp_directory_path().is_absolute());
  PASS();
}

// ==================== Coalescing Storm Tests ====================

static void test_coalesce_storm() {
  TEST("Coalesce: 1000 rapid writes to same file produce 1 event");
  FilesystemCoalescer coal(500);
  auto t = nowMs();

  for (int i = 0; i < 1000; ++i) {
    FilesystemEvent fe;
    fe.kind = FilesystemEventKind::FileModified;
    fe.path = "/tmp/storm.txt";
    fe.size = i * 10;
    coal.addEvent(std::move(fe), t + i);
  }

  ASSERT_EQ(coal.pendingCount(), 1u);
  auto flushed = coal.flush(t + 2000);
  ASSERT_EQ(flushed.size(), 1u);
  ASSERT_EQ(flushed[0].modification_count, 1000u);
  ASSERT_EQ(flushed[0].size, 9990u);
  PASS();
}

static void test_coalesce_storm_different_files_separate() {
  TEST("Coalesce: storm of different files stays separate");
  FilesystemCoalescer coal(500);
  auto t = nowMs();

  for (int i = 0; i < 100; ++i) {
    FilesystemEvent fe;
    fe.kind = FilesystemEventKind::FileModified;
    fe.path = "/tmp/file_" + std::to_string(i) + ".txt";
    coal.addEvent(std::move(fe), t);
  }

  ASSERT_EQ(coal.pendingCount(), 100u);
  PASS();
}

// ==================== Performance Tests ====================

static void test_perf_dedup_100k() {
  TEST("Perf: Deduplicator 100K events");
  FilesystemDeduplicator dedup(100);
  auto t = nowMs();
  auto start = std::chrono::steady_clock::now();
  for (int i = 0; i < 100000; ++i) {
    FilesystemEvent fe;
    fe.kind = FilesystemEventKind::FileModified;
    fe.path = "/tmp/file_" + std::to_string(i % 100) + ".txt";
    dedup.isDuplicate(fe, t + i);
  }
  auto end = std::chrono::steady_clock::now();
  double ms = std::chrono::duration<double, std::milli>(end - start).count();
  printf("(%.0f ms, %.0fK/s) ", ms, 100000.0 / ms);
  PASS();
}

static void test_perf_coalesce_100k() {
  TEST("Perf: Coalescer 100K events");
  FilesystemCoalescer coal(500);
  auto t = nowMs();
  auto start = std::chrono::steady_clock::now();
  for (int i = 0; i < 100000; ++i) {
    FilesystemEvent fe;
    fe.kind = FilesystemEventKind::FileModified;
    fe.path = "/tmp/file_" + std::to_string(i % 10) + ".txt";
    coal.addEvent(std::move(fe), t + i);
  }
  auto end = std::chrono::steady_clock::now();
  double ms = std::chrono::duration<double, std::milli>(end - start).count();
  ASSERT_EQ(coal.pendingCount(), 10u);
  printf("(%.0f ms, %.0fK/s) ", ms, 100000.0 / ms);
  PASS();
}

// ==================== Config Defaults Tests ====================

static void test_fs_config_defaults() {
  TEST("FilesystemConfig: defaults are safe");
  auto cfg = FilesystemConfig::defaults();
  ASSERT_TRUE(cfg.recursive);
  ASSERT_EQ(cfg.watcher_mode, WatcherMode::Native);
  ASSERT_EQ(cfg.polling_interval_ms, 5000u);
  ASSERT_EQ(cfg.hash_mode, FileHashMode::Disabled);
  ASSERT_EQ(cfg.coalesce_window_ms, 500u);
  ASSERT_EQ(cfg.dedup_window_ms, 200u);
  ASSERT_TRUE(cfg.normalize_paths);
  PASS();
}

// ==================== Main ====================

#ifndef MONIX_KERNEL_BUILD
int main() {
  printf("=== MONIX Filesystem Collector Tests ===\n\n");

  printf("[Enum Names]\n");
  test_event_kind_names();
  test_hash_mode_names();
  test_watcher_mode_names();
  test_hash_mode_from_string();

  printf("\n[Event Types]\n");
  test_event_type_file();
  test_event_type_directory();

  printf("\n[Deduplicator]\n");
  test_dedup_first_event_passes();
  test_dedup_same_event_duplicate();
  test_dedup_different_path_not_duplicate();
  test_dedup_different_kind_not_duplicate();
  test_dedup_after_window_passes();
  test_dedup_clear();

  printf("\n[Coalescer]\n");
  test_coalesce_first_event_new();
  test_coalesce_same_event_merges();
  test_coalesce_different_path_separate();
  test_coalesce_different_kind_separate();
  test_coalesce_flush_before_window();
  test_coalesce_flush_after_window();
  test_coalesce_hash_propagation();

  printf("\n[Coalescing Storm]\n");
  test_coalesce_storm();
  test_coalesce_storm_different_files_separate();

  printf("\n[Hasher]\n");
  test_hash_bytes_deterministic();
  test_hash_bytes_different_data();
  test_hash_file();
  test_hash_disabled_returns_empty();

  printf("\n[FilesystemCollector]\n");
  test_collector_lifecycle();
  test_collector_info();
  test_collector_capabilities();
  test_collector_start_duplicate();
  test_collector_stop_without_start();
  test_collector_collects_events();
  test_collector_delete_detection();
  test_collector_directory_detection();

  printf("\n[Path]\n");
  test_path_normalization();
  test_path_is_absolute();

  printf("\n[Config]\n");
  test_fs_config_defaults();

  printf("\n[Performance]\n");
  test_perf_dedup_100k();
  test_perf_coalesce_100k();

  printf("\n=== Results: %d passed, %d failed ===\n", gPassed, gFailed);
  return gFailed > 0 ? 1 : 0;
}
#endif

#ifdef MONIX_KERNEL_BUILD
int GetFailedCount_FilesystemTests() { return gFailed; }

struct KTestEntry {
  const char* display_name;
  void (*func)();
};

static const KTestEntry s_ktests[] = {
  {"FilesystemEventKind: names", test_event_kind_names},
  {"FileHashMode: names", test_hash_mode_names},
  {"WatcherMode: names", test_watcher_mode_names},
  {"FileHashMode: fromString", test_hash_mode_from_string},
  {"FilesystemEvent: file type", test_event_type_file},
  {"FilesystemEvent: directory type", test_event_type_directory},
  {"Dedup: first event passes", test_dedup_first_event_passes},
  {"Dedup: same event duplicate", test_dedup_same_event_duplicate},
  {"Dedup: different path", test_dedup_different_path_not_duplicate},
  {"Dedup: different kind", test_dedup_different_kind_not_duplicate},
  {"Dedup: after window passes", test_dedup_after_window_passes},
  {"Dedup: clear", test_dedup_clear},
  {"Coalesce: first event new", test_coalesce_first_event_new},
  {"Coalesce: same event merges", test_coalesce_same_event_merges},
  {"Coalesce: different path separate", test_coalesce_different_path_separate},
  {"Coalesce: different kind separate", test_coalesce_different_kind_separate},
  {"Coalesce: flush before window", test_coalesce_flush_before_window},
  {"Coalesce: flush after window", test_coalesce_flush_after_window},
  {"Coalesce: hash propagation", test_coalesce_hash_propagation},
  {"Coalesce: 1000 rapid writes", test_coalesce_storm},
  {"Coalesce: storm different files", test_coalesce_storm_different_files_separate},
  {"Hasher: bytes deterministic", test_hash_bytes_deterministic},
  {"Hasher: different data", test_hash_bytes_different_data},
  {"Hasher: file", test_hash_file},
  {"Hasher: disabled returns empty", test_hash_disabled_returns_empty},
  {"FilesystemCollector: lifecycle", test_collector_lifecycle},
  {"FilesystemCollector: info", test_collector_info},
  {"FilesystemCollector: capabilities", test_collector_capabilities},
  {"FilesystemCollector: start duplicate", test_collector_start_duplicate},
  {"FilesystemCollector: stop without start", test_collector_stop_without_start},
  {"FilesystemCollector: collects events", test_collector_collects_events},
  {"FilesystemCollector: delete detection", test_collector_delete_detection},
  {"FilesystemCollector: directory detection", test_collector_directory_detection},
  {"Path: normalization", test_path_normalization},
  {"Path: is absolute", test_path_is_absolute},
  {"FilesystemConfig: defaults", test_fs_config_defaults},
  {"Perf: dedup 100k", test_perf_dedup_100k},
  {"Perf: coalesce 100k", test_perf_coalesce_100k},
};

const KTestEntry* GetKTests_Filesystem() { return s_ktests; }
std::size_t GetKTestCount_Filesystem() { return sizeof(s_ktests) / sizeof(s_ktests[0]); }
#endif
