#include "../../../core/collectors/storage/StorageTypes.hpp"
#include "../../../core/collectors/storage/StorageCollector.hpp"

#include <cassert>
#include <cstdio>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <atomic>

using namespace monix::collectors::storage;

static int gPassed = 0;
static int gFailed = 0;

#define TEST(name) printf("  %-62s ", name);
#define PASS() do { printf("[PASS]\n"); gPassed++; } while(0)
#define FAIL(msg) do { printf("[FAIL] %s\n", msg); gFailed++; } while(0)
#define ASSERT_TRUE(e) do { if (!(e)) { FAIL(#e); return; } } while(0)
#define ASSERT_FALSE(e) do { if ((e)) { FAIL(#e); return; } } while(0)
#define ASSERT_EQ(a, b) do { if ((a) != (b)) { FAIL(#a " != " #b); return; } } while(0)
#define ASSERT_NE(a, b) do { if ((a) == (b)) { FAIL(#a " == " #b); return; } } while(0)

static StorageInfo makeDisk(const std::string& path, const std::string& model = "") {
  StorageInfo s;
  s.kind = StorageKind::PhysicalDisk;
  s.drive_type = StorageDriveType::Fixed;
  s.disk.device_path = path;
  s.disk.model = model;
  s.disk.vendor = "TestVendor";
  s.disk.total_bytes = 1000000000000ULL;
  s.timestamp_ms = 1000;
  return s;
}

static StorageInfo makeVolume(const std::string& letter, const std::string& fs = "NTFS") {
  StorageInfo s;
  s.kind = StorageKind::Volume;
  s.drive_type = StorageDriveType::Fixed;
  s.volume.drive_letter = letter;
  s.volume.file_system = fs;
  s.volume.volume_name = "TestVol";
  s.volume.total_bytes = 500000000000ULL;
  s.volume.free_bytes = 250000000000ULL;
  s.volume.serial_number = 12345;
  s.mounted = true;
  s.timestamp_ms = 1000;
  return s;
}

static StorageInfo makeExternal(const std::string& letter) {
  StorageInfo s;
  s.kind = StorageKind::ExternalStorage;
  s.drive_type = StorageDriveType::Removable;
  s.volume.drive_letter = letter;
  s.volume.file_system = "FAT32";
  s.volume.volume_name = "USB Drive";
  s.volume.total_bytes = 32000000000ULL;
  s.mounted = true;
  s.timestamp_ms = 1000;
  return s;
}

static StorageInfo makeVirtual(const std::string& path) {
  StorageInfo s;
  s.kind = StorageKind::VirtualStorage;
  s.drive_type = StorageDriveType::Fixed;
  s.disk.device_path = path;
  s.disk.model = "Virtual Disk";
  s.timestamp_ms = 1000;
  return s;
}

// ==================== StorageKind Tests ====================

static void test_storage_kind_names() {
  TEST("StorageKind: all names");
  ASSERT_EQ(std::string(StorageKindName(StorageKind::PhysicalDisk)), "PhysicalDisk");
  ASSERT_EQ(std::string(StorageKindName(StorageKind::Partition)), "Partition");
  ASSERT_EQ(std::string(StorageKindName(StorageKind::Volume)), "Volume");
  ASSERT_EQ(std::string(StorageKindName(StorageKind::ExternalStorage)), "ExternalStorage");
  ASSERT_EQ(std::string(StorageKindName(StorageKind::VirtualStorage)), "VirtualStorage");
  ASSERT_EQ(std::string(StorageKindName(StorageKind::Unknown)), "Unknown");
  PASS();
}

static void test_storage_kind_from_name() {
  TEST("StorageKindFromName: recognizes all kinds");
  ASSERT_EQ(StorageKindFromName("PhysicalDisk"), StorageKind::PhysicalDisk);
  ASSERT_EQ(StorageKindFromName("physical_disk"), StorageKind::PhysicalDisk);
  ASSERT_EQ(StorageKindFromName("Partition"), StorageKind::Partition);
  ASSERT_EQ(StorageKindFromName("Volume"), StorageKind::Volume);
  ASSERT_EQ(StorageKindFromName("ExternalStorage"), StorageKind::ExternalStorage);
  ASSERT_EQ(StorageKindFromName("VirtualStorage"), StorageKind::VirtualStorage);
  ASSERT_EQ(StorageKindFromName("unknown"), StorageKind::Unknown);
  PASS();
}

// ==================== StorageEventKind Tests ====================

static void test_event_kind_names() {
  TEST("StorageEventKind: all names");
  ASSERT_EQ(std::string(StorageEventKindName(StorageEventKind::DiskConnected)), "DiskConnected");
  ASSERT_EQ(std::string(StorageEventKindName(StorageEventKind::DiskDisconnected)), "DiskDisconnected");
  ASSERT_EQ(std::string(StorageEventKindName(StorageEventKind::VolumeMounted)), "VolumeMounted");
  ASSERT_EQ(std::string(StorageEventKindName(StorageEventKind::VolumeUnmounted)), "VolumeUnmounted");
  ASSERT_EQ(std::string(StorageEventKindName(StorageEventKind::VolumeChanged)), "VolumeChanged");
  PASS();
}

static void test_event_kind_actions() {
  TEST("StorageEventKind: action strings");
  ASSERT_EQ(StorageEventKindAction(StorageEventKind::DiskConnected), "disk.connected");
  ASSERT_EQ(StorageEventKindAction(StorageEventKind::DiskDisconnected), "disk.disconnected");
  ASSERT_EQ(StorageEventKindAction(StorageEventKind::VolumeMounted), "volume.mounted");
  ASSERT_EQ(StorageEventKindAction(StorageEventKind::VolumeUnmounted), "volume.unmounted");
  ASSERT_EQ(StorageEventKindAction(StorageEventKind::VolumeChanged), "volume.changed");
  PASS();
}

// ==================== StorageDriveType Tests ====================

static void test_drive_type_names() {
  TEST("StorageDriveType: all names");
  ASSERT_EQ(std::string(StorageDriveTypeName(StorageDriveType::Unknown)), "Unknown");
  ASSERT_EQ(std::string(StorageDriveTypeName(StorageDriveType::Removable)), "Removable");
  ASSERT_EQ(std::string(StorageDriveTypeName(StorageDriveType::Fixed)), "Fixed");
  ASSERT_EQ(std::string(StorageDriveTypeName(StorageDriveType::Remote)), "Remote");
  ASSERT_EQ(std::string(StorageDriveTypeName(StorageDriveType::CDROM)), "CDROM");
  ASSERT_EQ(std::string(StorageDriveTypeName(StorageDriveType::RAMDisk)), "RAMDisk");
  PASS();
}

// ==================== DetectionOrigin Tests ====================

static void test_detection_origin_names() {
  TEST("StorageDetectionOrigin: all names");
  ASSERT_EQ(std::string(StorageDetectionOriginName(StorageDetectionOrigin::WMI)), "WMI");
  ASSERT_EQ(std::string(StorageDetectionOriginName(StorageDetectionOrigin::DeviceIoControl)), "DeviceIoControl");
  ASSERT_EQ(std::string(StorageDetectionOriginName(StorageDetectionOrigin::Manual)), "Manual");
  ASSERT_EQ(std::string(StorageDetectionOriginName(StorageDetectionOrigin::Polling)), "Polling");
  PASS();
}

// ==================== DiskIdentity Tests ====================

static void test_disk_identity_defaults() {
  TEST("DiskIdentity: defaults are safe");
  DiskIdentity d;
  ASSERT_FALSE(d.hasDevicePath());
  ASSERT_FALSE(d.hasSerial());
  PASS();
}

static void test_disk_identity_has_fields() {
  TEST("DiskIdentity: hasDevicePath and hasSerial");
  DiskIdentity withPath;
  withPath.device_path = "\\\\.\\PhysicalDrive0";
  ASSERT_TRUE(withPath.hasDevicePath());

  DiskIdentity withSerial;
  withSerial.serial = "ABC123";
  ASSERT_TRUE(withSerial.hasSerial());
  PASS();
}

static void test_disk_identity_summary() {
  TEST("DiskIdentity: summary format");
  DiskIdentity full;
  full.vendor = "Samsung";
  full.model = "970 EVO";
  ASSERT_EQ(full.summary(), "Samsung 970 EVO");

  DiskIdentity modelOnly;
  modelOnly.model = "Generic SSD";
  ASSERT_EQ(modelOnly.summary(), "Generic SSD");

  DiskIdentity pathOnly;
  pathOnly.device_path = "\\\\.\\PhysicalDrive0";
  ASSERT_EQ(pathOnly.summary(), "\\\\.\\PhysicalDrive0");

  DiskIdentity empty;
  ASSERT_EQ(empty.summary(), "Unknown Disk");
  PASS();
}

// ==================== VolumeIdentity Tests ====================

static void test_volume_identity_defaults() {
  TEST("VolumeIdentity: defaults are safe");
  VolumeIdentity v;
  ASSERT_FALSE(v.hasDriveLetter());
  ASSERT_FALSE(v.hasMountPath());
  PASS();
}

static void test_volume_identity_summary() {
  TEST("VolumeIdentity: summary format");
  VolumeIdentity withLetter;
  withLetter.drive_letter = "C";
  withLetter.volume_name = "System";
  ASSERT_EQ(withLetter.summary(), "C (System)");

  VolumeIdentity letterOnly;
  letterOnly.drive_letter = "D";
  ASSERT_EQ(letterOnly.summary(), "D");

  VolumeIdentity mountOnly;
  mountOnly.mount_path = "/mnt/data";
  ASSERT_EQ(mountOnly.summary(), "/mnt/data");

  VolumeIdentity empty;
  ASSERT_EQ(empty.summary(), "Unknown Volume");
  PASS();
}

// ==================== StorageInfo Tests ====================

static void test_storage_info_is_valid() {
  TEST("StorageInfo: isValid checks fields");
  StorageInfo withId;
  withId.id = 42;
  ASSERT_TRUE(withId.isValid());

  StorageInfo withPath;
  withPath.disk.device_path = "\\\\.\\PhysicalDrive0";
  ASSERT_TRUE(withPath.isValid());

  StorageInfo withLetter;
  withLetter.volume.drive_letter = "C";
  ASSERT_TRUE(withLetter.isValid());

  StorageInfo empty;
  ASSERT_FALSE(empty.isValid());
  PASS();
}

static void test_storage_info_is_disk() {
  TEST("StorageInfo: isDisk for physical and partition");
  StorageInfo physical;
  physical.kind = StorageKind::PhysicalDisk;
  ASSERT_TRUE(physical.isDisk());

  StorageInfo partition;
  partition.kind = StorageKind::Partition;
  ASSERT_TRUE(partition.isDisk());

  StorageInfo volume;
  volume.kind = StorageKind::Volume;
  ASSERT_FALSE(volume.isDisk());
  PASS();
}

static void test_storage_info_is_volume() {
  TEST("StorageInfo: isVolume for volume and external");
  StorageInfo vol;
  vol.kind = StorageKind::Volume;
  ASSERT_TRUE(vol.isVolume());

  StorageInfo ext;
  ext.kind = StorageKind::ExternalStorage;
  ASSERT_TRUE(ext.isVolume());

  StorageInfo physical;
  physical.kind = StorageKind::PhysicalDisk;
  ASSERT_FALSE(physical.isVolume());
  PASS();
}

// ==================== Collector Lifecycle Tests ====================

static void test_collector_start_stop() {
  TEST("StorageCollector: start and stop lifecycle");
  StorageCollector collector;
  ASSERT_FALSE(collector.isRunning());
  ASSERT_TRUE(collector.start());
  ASSERT_TRUE(collector.isRunning());
  ASSERT_TRUE(collector.stop());
  ASSERT_FALSE(collector.isRunning());
  PASS();
}

static void test_collector_double_start() {
  TEST("StorageCollector: double start returns false");
  StorageCollector collector;
  ASSERT_TRUE(collector.start());
  ASSERT_FALSE(collector.start());
  ASSERT_TRUE(collector.stop());
  PASS();
}

static void test_collector_stop_without_start() {
  TEST("StorageCollector: stop without start returns false");
  StorageCollector collector;
  ASSERT_FALSE(collector.stop());
  PASS();
}

static void test_collector_config_defaults() {
  TEST("StorageCollectorConfig: defaults are safe");
  auto cfg = StorageCollectorConfig::defaults();
  ASSERT_TRUE(cfg.track_physical_disk);
  ASSERT_TRUE(cfg.track_volumes);
  ASSERT_TRUE(cfg.track_external);
  ASSERT_TRUE(cfg.track_virtual);
  ASSERT_TRUE(cfg.max_events > 0);
  PASS();
}

// ==================== Mount Tests ====================

static void test_volume_mount() {
  TEST("Mount: volume mount detected");
  StorageCollector collector;
  std::atomic<int> count{0};
  StorageEventKind capturedKind = StorageEventKind::VolumeUnmounted;

  collector.setCallback([&](StorageEventKind kind, StorageDetectionOrigin origin, const StorageInfo& info) {
    count++;
    capturedKind = kind;
  });

  StorageCollectorConfig cfg = StorageCollectorConfig::defaults();
  cfg.auto_enumerate = false;
  collector.start(cfg);
  StorageInfo vol = makeVolume("E");
  collector.reportVolumeMounted(vol);

  ASSERT_EQ(count.load(), 1);
  ASSERT_EQ(capturedKind, StorageEventKind::VolumeMounted);
  ASSERT_EQ(collector.volumesTracked(), 1u);
  collector.stop();
  PASS();
}

// ==================== Unmount Tests ====================

static void test_volume_unmount() {
  TEST("Unmount: volume unmount detected");
  StorageCollector collector;
  std::atomic<int> count{0};
  StorageEventKind capturedKind = StorageEventKind::VolumeMounted;

  collector.setCallback([&](StorageEventKind kind, StorageDetectionOrigin origin, const StorageInfo& info) {
    count++;
    capturedKind = kind;
  });

  StorageCollectorConfig cfg = StorageCollectorConfig::defaults();
  cfg.auto_enumerate = false;
  collector.start(cfg);

  StorageInfo vol = makeVolume("E");
  collector.reportVolumeMounted(vol);
  ASSERT_EQ(collector.volumesTracked(), 1u);

  collector.reportVolumeUnmounted(vol);
  ASSERT_EQ(count.load(), 2);
  ASSERT_EQ(capturedKind, StorageEventKind::VolumeUnmounted);
  ASSERT_EQ(collector.volumesTracked(), 0u);
  collector.stop();
  PASS();
}

// ==================== Disk Connect Tests ====================

static void test_disk_connect() {
  TEST("Disk connect: disk connection detected");
  StorageCollector collector;
  std::atomic<int> count{0};
  StorageEventKind capturedKind = StorageEventKind::DiskDisconnected;

  collector.setCallback([&](StorageEventKind kind, StorageDetectionOrigin origin, const StorageInfo& info) {
    count++;
    capturedKind = kind;
  });

  StorageCollectorConfig cfg = StorageCollectorConfig::defaults();
  cfg.auto_enumerate = false;
  collector.start(cfg);
  StorageInfo disk = makeDisk("\\\\.\\PhysicalDrive1");
  collector.reportDiskConnected(disk);

  ASSERT_EQ(count.load(), 1);
  ASSERT_EQ(capturedKind, StorageEventKind::DiskConnected);
  ASSERT_EQ(collector.disksTracked(), 1u);
  collector.stop();
  PASS();
}

// ==================== Disk Disconnect Tests ====================

static void test_disk_disconnect() {
  TEST("Disk disconnect: disk disconnection detected");
  StorageCollector collector;
  std::atomic<int> count{0};
  StorageEventKind capturedKind = StorageEventKind::DiskConnected;

  collector.setCallback([&](StorageEventKind kind, StorageDetectionOrigin origin, const StorageInfo& info) {
    count++;
    capturedKind = kind;
  });

  StorageCollectorConfig cfg = StorageCollectorConfig::defaults();
  cfg.auto_enumerate = false;
  collector.start(cfg);

  StorageInfo disk = makeDisk("\\\\.\\PhysicalDrive1");
  collector.reportDiskConnected(disk);
  ASSERT_EQ(collector.disksTracked(), 1u);

  collector.reportDiskDisconnected(disk);
  ASSERT_EQ(count.load(), 2);
  ASSERT_EQ(capturedKind, StorageEventKind::DiskDisconnected);
  ASSERT_EQ(collector.disksTracked(), 0u);
  collector.stop();
  PASS();
}

// ==================== Duplicate Tests ====================

static void test_duplicate_mount_suppressed() {
  TEST("Duplicate: rapid duplicate mount suppressed");
  StorageCollector collector;
  std::atomic<int> count{0};

  collector.setCallback([&](StorageEventKind kind, StorageDetectionOrigin origin, const StorageInfo& info) {
    count++;
  });

  collector.start();

  StorageInfo vol = makeVolume("E");
  collector.reportVolumeMounted(vol);
  collector.reportVolumeMounted(vol);
  collector.reportVolumeMounted(vol);

  ASSERT_EQ(count.load(), 1);
  collector.stop();
  PASS();
}

static void test_different_volumes_not_deduped() {
  TEST("Duplicate: different volumes not suppressed");
  StorageCollector collector;
  std::atomic<int> count{0};

  collector.setCallback([&](StorageEventKind kind, StorageDetectionOrigin origin, const StorageInfo& info) {
    count++;
  });

  collector.start();

  StorageInfo vol1 = makeVolume("E");
  StorageInfo vol2 = makeVolume("F");
  collector.reportVolumeMounted(vol1);
  collector.reportVolumeMounted(vol2);

  ASSERT_EQ(count.load(), 2);
  collector.stop();
  PASS();
}

// ==================== Snapshot Tests ====================

static void test_enumerate_volumes() {
  TEST("Snapshot: enumerate volumes finds current drives");
  StorageCollector collector;
  collector.start();

  auto volumes = collector.enumerateVolumes();
  ASSERT_TRUE(volumes.size() > 0);

  bool foundC = false;
  for (const auto& v : volumes) {
    if (v.volume.drive_letter == "C") {
      foundC = true;
      break;
    }
  }
  ASSERT_TRUE(foundC);
  collector.stop();
  PASS();
}

static void test_auto_enumerate_populates() {
  TEST("Snapshot: auto_enumerate populates tracked volumes");
  StorageCollector collector;
  collector.start();

  ASSERT_TRUE(collector.volumesTracked() > 0);
  collector.stop();
  PASS();
}

// ==================== Volume Changed Tests ====================

static void test_volume_changed() {
  TEST("Volume changed: change detected");
  StorageCollector collector;
  std::atomic<int> count{0};
  StorageEventKind capturedKind = StorageEventKind::VolumeMounted;

  collector.setCallback([&](StorageEventKind kind, StorageDetectionOrigin origin, const StorageInfo& info) {
    count++;
    capturedKind = kind;
  });

  collector.start();

  StorageInfo vol = makeVolume("E");
  collector.reportVolumeMounted(vol);
  collector.reportVolumeChanged(vol);

  ASSERT_TRUE(count.load() >= 2);
  ASSERT_EQ(capturedKind, StorageEventKind::VolumeChanged);
  collector.stop();
  PASS();
}

// ==================== Correlation Tests ====================

static void test_disk_volume_correlation() {
  TEST("Correlation: disk and volume share correlation fields");
  StorageCollector collector;
  std::string capturedDiskPath;
  std::string capturedVolumeLetter;

  collector.setCallback([&](StorageEventKind kind, StorageDetectionOrigin origin, const StorageInfo& info) {
    capturedDiskPath = info.correlated_disk_path;
    capturedVolumeLetter = info.correlated_volume_letter;
  });

  collector.start();

  StorageInfo vol = makeVolume("E");
  vol.correlated_disk_path = "\\\\.\\PhysicalDrive1";
  vol.correlated_volume_letter = "E";
  collector.reportVolumeMounted(vol);

  ASSERT_EQ(capturedDiskPath, "\\\\.\\PhysicalDrive1");
  ASSERT_EQ(capturedVolumeLetter, "E");
  collector.stop();
  PASS();
}

// ==================== External Storage Tests ====================

static void test_external_storage_detected() {
  TEST("External: USB drive detected as ExternalStorage");
  StorageCollector collector;
  StorageKind capturedKind = StorageKind::Unknown;

  collector.setCallback([&](StorageEventKind kind, StorageDetectionOrigin origin, const StorageInfo& info) {
    capturedKind = info.kind;
  });

  collector.start();
  StorageInfo ext = makeExternal("G");
  collector.reportVolumeMounted(ext);

  ASSERT_EQ(capturedKind, StorageKind::ExternalStorage);
  collector.stop();
  PASS();
}

// ==================== Virtual Storage Tests ====================

static void test_virtual_storage_detected() {
  TEST("Virtual: virtual disk detected as VirtualStorage");
  StorageCollector collector;
  StorageKind capturedKind = StorageKind::Unknown;

  collector.setCallback([&](StorageEventKind kind, StorageDetectionOrigin origin, const StorageInfo& info) {
    capturedKind = info.kind;
  });

  collector.start();
  StorageInfo virt = makeVirtual("\\\\.\\VirtualDrive0");
  collector.reportDiskConnected(virt);

  ASSERT_EQ(capturedKind, StorageKind::VirtualStorage);
  collector.stop();
  PASS();
}

// ==================== Permission Tests ====================

static void test_report_when_not_running() {
  TEST("Permission: report when not running ignored");
  StorageCollector collector;
  std::atomic<int> count{0};

  collector.setCallback([&](StorageEventKind kind, StorageDetectionOrigin origin, const StorageInfo& info) {
    count++;
  });

  StorageInfo vol = makeVolume("E");
  collector.reportVolumeMounted(vol);
  collector.reportVolumeUnmounted(vol);
  collector.reportVolumeChanged(vol);

  StorageInfo disk = makeDisk("\\\\.\\PhysicalDrive1");
  collector.reportDiskConnected(disk);
  collector.reportDiskDisconnected(disk);

  ASSERT_EQ(count.load(), 0);
  PASS();
}

// ==================== Config Filtering Tests ====================

static void test_config_disable_volumes() {
  TEST("Config: disable volume tracking");
  StorageCollectorConfig cfg = StorageCollectorConfig::defaults();
  cfg.track_volumes = false;
  cfg.track_external = false;
  cfg.auto_enumerate = false;

  StorageCollector collector;
  std::atomic<int> count{0};

  collector.setCallback([&](StorageEventKind kind, StorageDetectionOrigin origin, const StorageInfo& info) {
    count++;
  });

  collector.start(cfg);

  StorageInfo vol = makeVolume("E");
  collector.reportVolumeMounted(vol);

  ASSERT_EQ(count.load(), 0);
  ASSERT_EQ(collector.volumesTracked(), 0u);
  collector.stop();
  PASS();
}

static void test_config_disable_physical_disk() {
  TEST("Config: disable physical disk tracking");
  StorageCollectorConfig cfg = StorageCollectorConfig::defaults();
  cfg.track_physical_disk = false;
  cfg.auto_enumerate = false;

  StorageCollector collector;
  std::atomic<int> count{0};

  collector.setCallback([&](StorageEventKind kind, StorageDetectionOrigin origin, const StorageInfo& info) {
    count++;
  });

  collector.start(cfg);

  StorageInfo disk = makeDisk("\\\\.\\PhysicalDrive1");
  collector.reportDiskConnected(disk);

  ASSERT_EQ(count.load(), 0);
  ASSERT_EQ(collector.disksTracked(), 0u);
  collector.stop();
  PASS();
}

// ==================== Event Log Tests ====================

static void test_event_log_accumulates() {
  TEST("Event log: events accumulate");
  StorageCollector collector;
  collector.start();

  for (int i = 0; i < 3; i++) {
    StorageInfo vol = makeVolume(std::string(1, 'E' + i));
    collector.reportVolumeMounted(vol);
  }

  auto recent = collector.recentEvents(10);
  ASSERT_TRUE(recent.size() == 3);
  ASSERT_EQ(collector.eventsEmitted(), 3u);
  collector.stop();
  PASS();
}

// ==================== Main ====================

#ifndef MONIX_KERNEL_BUILD
int main() {
  printf("=== MONIX Storage Collector Tests ===\n\n");

  printf("[StorageKind]\n");
  test_storage_kind_names();
  test_storage_kind_from_name();

  printf("[StorageEventKind]\n");
  test_event_kind_names();
  test_event_kind_actions();

  printf("[StorageDriveType]\n");
  test_drive_type_names();

  printf("[DetectionOrigin]\n");
  test_detection_origin_names();

  printf("[DiskIdentity]\n");
  test_disk_identity_defaults();
  test_disk_identity_has_fields();
  test_disk_identity_summary();

  printf("[VolumeIdentity]\n");
  test_volume_identity_defaults();
  test_volume_identity_summary();

  printf("[StorageInfo]\n");
  test_storage_info_is_valid();
  test_storage_info_is_disk();
  test_storage_info_is_volume();

  printf("[Collector Lifecycle]\n");
  test_collector_start_stop();
  test_collector_double_start();
  test_collector_stop_without_start();
  test_collector_config_defaults();

  printf("[Mount]\n");
  test_volume_mount();

  printf("[Unmount]\n");
  test_volume_unmount();

  printf("[Disk Connect]\n");
  test_disk_connect();

  printf("[Disk Disconnect]\n");
  test_disk_disconnect();

  printf("[Duplicate]\n");
  test_duplicate_mount_suppressed();
  test_different_volumes_not_deduped();

  printf("[Snapshot]\n");
  test_enumerate_volumes();
  test_auto_enumerate_populates();

  printf("[Volume Changed]\n");
  test_volume_changed();

  printf("[Correlation]\n");
  test_disk_volume_correlation();

  printf("[External Storage]\n");
  test_external_storage_detected();

  printf("[Virtual Storage]\n");
  test_virtual_storage_detected();

  printf("[Permission]\n");
  test_report_when_not_running();

  printf("[Config Filtering]\n");
  test_config_disable_volumes();
  test_config_disable_physical_disk();

  printf("[Event Log]\n");
  test_event_log_accumulates();

  printf("\n=== Results: %d passed, %d failed ===\n", gPassed, gFailed);
  return gFailed > 0 ? 1 : 0;
}
#endif

#ifdef MONIX_KERNEL_BUILD
int GetFailedCount_StorageCollectorTests() { return gFailed; }

struct KTestEntry {
  const char* display_name;
  void (*func)();
};

static const KTestEntry s_ktests[] = {
  {"StorageKind: names", test_storage_kind_names},
  {"StorageKindFromName: all", test_storage_kind_from_name},
  {"StorageEventKind: names", test_event_kind_names},
  {"StorageEventKind: actions", test_event_kind_actions},
  {"StorageDriveType: names", test_drive_type_names},
  {"StorageDetectionOrigin: names", test_detection_origin_names},
  {"DiskIdentity: defaults", test_disk_identity_defaults},
  {"DiskIdentity: has fields", test_disk_identity_has_fields},
  {"DiskIdentity: summary", test_disk_identity_summary},
  {"VolumeIdentity: defaults", test_volume_identity_defaults},
  {"VolumeIdentity: summary", test_volume_identity_summary},
  {"StorageInfo: isValid", test_storage_info_is_valid},
  {"StorageInfo: isDisk", test_storage_info_is_disk},
  {"StorageInfo: isVolume", test_storage_info_is_volume},
  {"StorageCollector: start stop", test_collector_start_stop},
  {"StorageCollector: double start", test_collector_double_start},
  {"StorageCollector: stop without start", test_collector_stop_without_start},
  {"StorageCollectorConfig: defaults", test_collector_config_defaults},
  {"Mount: detection", test_volume_mount},
  {"Unmount: detection", test_volume_unmount},
  {"Disk connect: detection", test_disk_connect},
  {"Disk disconnect: detection", test_disk_disconnect},
  {"Duplicate: suppressed", test_duplicate_mount_suppressed},
  {"Duplicate: different not deduped", test_different_volumes_not_deduped},
  {"Snapshot: enumerate volumes", test_enumerate_volumes},
  {"Snapshot: auto_enumerate", test_auto_enumerate_populates},
  {"Volume changed: detection", test_volume_changed},
  {"Correlation: disk and volume", test_disk_volume_correlation},
  {"External: USB detected", test_external_storage_detected},
  {"Virtual: detected", test_virtual_storage_detected},
  {"Permission: not running ignored", test_report_when_not_running},
  {"Config: disable volumes", test_config_disable_volumes},
  {"Config: disable physical disk", test_config_disable_physical_disk},
  {"Event log: accumulates", test_event_log_accumulates},
};

const KTestEntry* GetKTests_StorageCollector() { return s_ktests; }
std::size_t GetKTestCount_StorageCollector() { return sizeof(s_ktests) / sizeof(s_ktests[0]); }
#endif
