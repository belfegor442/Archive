#include "../../../core/collectors/device/DeviceTypes.hpp"
#include "../../../core/collectors/device/DeviceCollector.hpp"

#include <cassert>
#include <cstdio>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <atomic>

using namespace monix::collectors::device;

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

static DeviceInfo makeDevice(const std::string& name, DeviceClass cls, DeviceOrigin origin,
                              const std::string& vid = "", const std::string& pid = "",
                              const std::string& serial = "", const std::string& iid = "") {
  DeviceInfo d;
  d.identity.friendly_name = name;
  d.device_class = cls;
  d.origin = origin;
  d.identity.vendor_id = vid;
  d.identity.product_id = pid;
  d.identity.serial = serial;
  d.identity.instance_id = iid;
  d.timestamp_ms = 1000;
  return d;
}

// ==================== DeviceClassName Tests ====================

static void test_device_class_names() {
  TEST("DeviceClass: all names");
  ASSERT_EQ(std::string(DeviceClassName(DeviceClass::USB)), "USB");
  ASSERT_EQ(std::string(DeviceClassName(DeviceClass::PCIe)), "PCIe");
  ASSERT_EQ(std::string(DeviceClassName(DeviceClass::Bluetooth)), "Bluetooth");
  ASSERT_EQ(std::string(DeviceClassName(DeviceClass::Storage)), "Storage");
  ASSERT_EQ(std::string(DeviceClassName(DeviceClass::Display)), "Display");
  ASSERT_EQ(std::string(DeviceClassName(DeviceClass::Audio)), "Audio");
  ASSERT_EQ(std::string(DeviceClassName(DeviceClass::Network)), "Network");
  ASSERT_EQ(std::string(DeviceClassName(DeviceClass::Input)), "Input");
  ASSERT_EQ(std::string(DeviceClassName(DeviceClass::Camera)), "Camera");
  ASSERT_EQ(std::string(DeviceClassName(DeviceClass::Printer)), "Printer");
  ASSERT_EQ(std::string(DeviceClassName(DeviceClass::Other)), "Other");
  ASSERT_EQ(std::string(DeviceClassName(DeviceClass::Unknown)), "Unknown");
  PASS();
}

// ==================== DeviceClassFromName Tests ====================

static void test_class_from_name() {
  TEST("DeviceClassFromName: recognizes all classes");
  ASSERT_EQ(DeviceClassFromName("usb"), DeviceClass::USB);
  ASSERT_EQ(DeviceClassFromName("USB"), DeviceClass::USB);
  ASSERT_EQ(DeviceClassFromName("pcie"), DeviceClass::PCIe);
  ASSERT_EQ(DeviceClassFromName("bluetooth"), DeviceClass::Bluetooth);
  ASSERT_EQ(DeviceClassFromName("storage"), DeviceClass::Storage);
  ASSERT_EQ(DeviceClassFromName("disk"), DeviceClass::Storage);
  ASSERT_EQ(DeviceClassFromName("display"), DeviceClass::Display);
  ASSERT_EQ(DeviceClassFromName("video"), DeviceClass::Display);
  ASSERT_EQ(DeviceClassFromName("audio"), DeviceClass::Audio);
  ASSERT_EQ(DeviceClassFromName("network"), DeviceClass::Network);
  ASSERT_EQ(DeviceClassFromName("wifi"), DeviceClass::Network);
  ASSERT_EQ(DeviceClassFromName("input"), DeviceClass::Input);
  ASSERT_EQ(DeviceClassFromName("hid"), DeviceClass::Input);
  ASSERT_EQ(DeviceClassFromName("camera"), DeviceClass::Camera);
  ASSERT_EQ(DeviceClassFromName("printer"), DeviceClass::Printer);
  ASSERT_EQ(DeviceClassFromName("unknown_class"), DeviceClass::Unknown);
  PASS();
}

// ==================== DeviceClassFromInstanceId Tests ====================

static void test_class_from_instance_id() {
  TEST("DeviceClassFromInstanceId: detects from instance paths");
  ASSERT_EQ(DeviceClassFromInstanceId("USB\\VID_1234&PID_5678\\aabb"), DeviceClass::USB);
  ASSERT_EQ(DeviceClassFromInstanceId("USB\\VID_1234&PID_5678\\class_07"), DeviceClass::Storage);
  ASSERT_EQ(DeviceClassFromInstanceId("USB\\VID_1234&PID_5678\\class_03"), DeviceClass::Display);
  ASSERT_EQ(DeviceClassFromInstanceId("USB\\VID_1234&PID_5678\\class_04"), DeviceClass::Audio);
  ASSERT_EQ(DeviceClassFromInstanceId("USB\\VID_1234&PID_5678\\class_01"), DeviceClass::Input);
  ASSERT_EQ(DeviceClassFromInstanceId("USB\\VID_1234&PID_5678\\class_0e"), DeviceClass::Camera);
  ASSERT_EQ(DeviceClassFromInstanceId("BTH\\RFBUS\\00000000"), DeviceClass::Bluetooth);
  ASSERT_EQ(DeviceClassFromInstanceId("PCI\\VEN_8086\\DEV_1234"), DeviceClass::PCIe);
  ASSERT_EQ(DeviceClassFromInstanceId("DISPLAY\\DEL4123\\4&1234"), DeviceClass::Display);
  ASSERT_EQ(DeviceClassFromInstanceId("ROOT\\VMBUS\\0000"), DeviceClass::Unknown);
  PASS();
}

// ==================== DeviceClassFromGuid Tests ====================

static void test_class_from_guid() {
  TEST("DeviceClassFromGuid: recognizes Windows class GUIDs");
  ASSERT_EQ(DeviceClassFromGuid("4D36E967-E325-11CE-BFC1-08002BE10318"), DeviceClass::Storage);
  ASSERT_EQ(DeviceClassFromGuid("4D36E968-E325-11CE-BFC1-08002BE10318"), DeviceClass::Display);
  ASSERT_EQ(DeviceClassFromGuid("4D36E96C-E325-11CE-BFC1-08002BE10318"), DeviceClass::Audio);
  ASSERT_EQ(DeviceClassFromGuid("4D36E96E-E325-11CE-BFC1-08002BE10318"), DeviceClass::Network);
  ASSERT_EQ(DeviceClassFromGuid("4D36E96B-E325-11CE-BFC1-08002BE10318"), DeviceClass::Input);
  ASSERT_EQ(DeviceClassFromGuid("6bdd1fc6-810f-11d0-bec7-08002be2092f"), DeviceClass::Camera);
  ASSERT_EQ(DeviceClassFromGuid("4D36E979-E325-11CE-BFC1-08002BE10318"), DeviceClass::Printer);
  ASSERT_EQ(DeviceClassFromGuid("00000000-0000-0000-0000-000000000000"), DeviceClass::Unknown);
  PASS();
}

// ==================== DeviceOrigin Tests ====================

static void test_origin_names() {
  TEST("DeviceOrigin: all names");
  ASSERT_EQ(std::string(DeviceOriginName(DeviceOrigin::Internal)), "Internal");
  ASSERT_EQ(std::string(DeviceOriginName(DeviceOrigin::External)), "External");
  ASSERT_EQ(std::string(DeviceOriginName(DeviceOrigin::Virtual)), "Virtual");
  ASSERT_EQ(std::string(DeviceOriginName(DeviceOrigin::Unknown)), "Unknown");
  PASS();
}

static void test_detect_origin() {
  TEST("DetectDeviceOrigin: detects from instance_id and bus");
  ASSERT_EQ(DetectDeviceOrigin("USB\\VID_1234&PID_5678\\aabb", ""), DeviceOrigin::External);
  ASSERT_EQ(DetectDeviceOrigin("", "USB"), DeviceOrigin::External);
  ASSERT_EQ(DetectDeviceOrigin("PCI\\VEN_8086\\DEV_1234", "PCIe"), DeviceOrigin::Internal);
  ASSERT_EQ(DetectDeviceOrigin("", "PCIe"), DeviceOrigin::Internal);
  ASSERT_EQ(DetectDeviceOrigin("ROOT\\VMBUS\\0000", ""), DeviceOrigin::Virtual);
  ASSERT_EQ(DetectDeviceOrigin("", "Virtual"), DeviceOrigin::Virtual);
  ASSERT_EQ(DetectDeviceOrigin("", ""), DeviceOrigin::Unknown);
  PASS();
}

// ==================== DeviceEventKind Tests ====================

static void test_event_kind_names() {
  TEST("DeviceEventKind: all names");
  ASSERT_EQ(std::string(DeviceEventKindName(DeviceEventKind::Connected)), "Connected");
  ASSERT_EQ(std::string(DeviceEventKindName(DeviceEventKind::Disconnected)), "Disconnected");
  ASSERT_EQ(std::string(DeviceEventKindName(DeviceEventKind::Changed)), "Changed");
  ASSERT_EQ(std::string(DeviceEventKindName(DeviceEventKind::Available)), "Available");
  ASSERT_EQ(std::string(DeviceEventKindName(DeviceEventKind::Unavailable)), "Unavailable");
  PASS();
}

static void test_event_kind_actions() {
  TEST("DeviceEventKind: action strings");
  ASSERT_EQ(DeviceEventKindAction(DeviceEventKind::Connected), "connected");
  ASSERT_EQ(DeviceEventKindAction(DeviceEventKind::Disconnected), "disconnected");
  ASSERT_EQ(DeviceEventKindAction(DeviceEventKind::Changed), "changed");
  ASSERT_EQ(DeviceEventKindAction(DeviceEventKind::Available), "available");
  ASSERT_EQ(DeviceEventKindAction(DeviceEventKind::Unavailable), "unavailable");
  PASS();
}

// ==================== DetectionOrigin Tests ====================

static void test_detection_origin_names() {
  TEST("DeviceDetectionOrigin: all names");
  ASSERT_EQ(std::string(DeviceDetectionOriginName(DeviceDetectionOrigin::WMI)), "WMI");
  ASSERT_EQ(std::string(DeviceDetectionOriginName(DeviceDetectionOrigin::SetupAPI)), "SetupAPI");
  ASSERT_EQ(std::string(DeviceDetectionOriginName(DeviceDetectionOrigin::Manual)), "Manual");
  ASSERT_EQ(std::string(DeviceDetectionOriginName(DeviceDetectionOrigin::Polling)), "Polling");
  PASS();
}

// ==================== DeviceIdentity Tests ====================

static void test_device_identity_defaults() {
  TEST("DeviceIdentity: defaults are safe");
  DeviceIdentity id;
  ASSERT_FALSE(id.hasIds());
  ASSERT_FALSE(id.hasInstanceId());
  PASS();
}

static void test_device_identity_has_ids() {
  TEST("DeviceIdentity: hasIds checks vendor and product");
  DeviceIdentity withIds;
  withIds.vendor_id = "1234";
  withIds.product_id = "5678";
  ASSERT_TRUE(withIds.hasIds());

  DeviceIdentity noVendor;
  noVendor.product_id = "5678";
  ASSERT_FALSE(noVendor.hasIds());

  DeviceIdentity noProduct;
  noProduct.vendor_id = "1234";
  ASSERT_FALSE(noProduct.hasIds());
  PASS();
}

static void test_device_identity_summary() {
  TEST("DeviceIdentity: summary prioritizes fields");
  DeviceIdentity withName;
  withName.friendly_name = "USB Mouse";
  ASSERT_EQ(withName.summary(), "USB Mouse");

  DeviceIdentity withDesc;
  withDesc.description = "HID-compliant mouse";
  ASSERT_EQ(withDesc.summary(), "HID-compliant mouse");

  DeviceIdentity withIds;
  withIds.vendor_id = "1234";
  withIds.product_id = "5678";
  ASSERT_EQ(withIds.summary(), "1234:5678");

  DeviceIdentity withIid;
  withIid.instance_id = "USB\\VID_1234&PID_5678";
  ASSERT_EQ(withIid.summary(), "USB\\VID_1234&PID_5678");

  DeviceIdentity empty;
  ASSERT_EQ(empty.summary(), "Unknown Device");
  PASS();
}

// ==================== DeviceInfo Tests ====================

static void test_device_info_defaults() {
  TEST("DeviceInfo: defaults are safe");
  DeviceInfo d;
  ASSERT_EQ(d.id, 0u);
  ASSERT_EQ(d.device_class, DeviceClass::Unknown);
  ASSERT_EQ(d.origin, DeviceOrigin::Unknown);
  ASSERT_FALSE(d.connected);
  ASSERT_EQ(d.timestamp_ms, 0);
  PASS();
}

static void test_device_info_is_valid() {
  TEST("DeviceInfo: isValid checks id or instance_id");
  DeviceInfo withId;
  withId.id = 42;
  ASSERT_TRUE(withId.isValid());

  DeviceInfo withIid;
  withIid.identity.instance_id = "USB\\VID_1234";
  ASSERT_TRUE(withIid.isValid());

  DeviceInfo empty;
  ASSERT_FALSE(empty.isValid());
  PASS();
}

static void test_device_info_is_same_device() {
  TEST("DeviceInfo: isSameDevice compares id, instance_id, or ids");
  DeviceInfo a, b;

  a.id = 42;
  b.id = 42;
  ASSERT_TRUE(a.isSameDevice(b));

  b.id = 99;
  ASSERT_FALSE(a.isSameDevice(b));

  DeviceInfo c, d;
  c.identity.instance_id = "USB\\VID_1234";
  d.identity.instance_id = "USB\\VID_1234";
  ASSERT_TRUE(c.isSameDevice(d));

  d.identity.instance_id = "USB\\VID_5678";
  ASSERT_FALSE(c.isSameDevice(d));

  DeviceInfo e, f;
  e.identity.vendor_id = "1234";
  e.identity.product_id = "5678";
  f.identity.vendor_id = "1234";
  f.identity.product_id = "5678";
  ASSERT_TRUE(e.isSameDevice(f));
  PASS();
}

// ==================== Collector Lifecycle Tests ====================

static void test_collector_start_stop() {
  TEST("DeviceCollector: start and stop lifecycle");
  DeviceCollector collector;
  ASSERT_FALSE(collector.isRunning());
  ASSERT_TRUE(collector.start());
  ASSERT_TRUE(collector.isRunning());
  ASSERT_TRUE(collector.stop());
  ASSERT_FALSE(collector.isRunning());
  PASS();
}

static void test_collector_double_start() {
  TEST("DeviceCollector: double start returns false");
  DeviceCollector collector;
  ASSERT_TRUE(collector.start());
  ASSERT_FALSE(collector.start());
  ASSERT_TRUE(collector.stop());
  PASS();
}

static void test_collector_stop_without_start() {
  TEST("DeviceCollector: stop without start returns false");
  DeviceCollector collector;
  ASSERT_FALSE(collector.stop());
  PASS();
}

static void test_collector_config_defaults() {
  TEST("DeviceCollectorConfig: defaults are safe");
  auto cfg = DeviceCollectorConfig::defaults();
  ASSERT_TRUE(cfg.track_usb);
  ASSERT_TRUE(cfg.track_bluetooth);
  ASSERT_TRUE(cfg.track_storage);
  ASSERT_TRUE(cfg.track_display);
  ASSERT_TRUE(cfg.dedup_window_ms > 0);
  ASSERT_TRUE(cfg.max_devices > 0);
  PASS();
}

// ==================== Connect Tests ====================

static void test_connect_detection() {
  TEST("Connect: device connection detected");
  DeviceCollector collector;
  std::atomic<int> count{0};
  DeviceEventKind capturedKind = DeviceEventKind::Disconnected;

  collector.setCallback([&](DeviceEventKind kind, DeviceDetectionOrigin origin, const DeviceInfo& info) {
    count++;
    capturedKind = kind;
  });

  collector.start();
  DeviceInfo usb = makeDevice("USB Drive", DeviceClass::Storage, DeviceOrigin::External,
                               "1234", "5678", "SN001", "USB\\VID_1234&PID_5678\\SN001");
  collector.reportConnection(usb);

  ASSERT_EQ(count.load(), 1);
  ASSERT_EQ(capturedKind, DeviceEventKind::Connected);
  ASSERT_EQ(collector.devicesTracked(), 1u);
  collector.stop();
  PASS();
}

static void test_connect_multiple_devices() {
  TEST("Connect: multiple different devices tracked");
  DeviceCollector collector;
  std::atomic<int> count{0};

  collector.setCallback([&](DeviceEventKind kind, DeviceDetectionOrigin origin, const DeviceInfo& info) {
    count++;
  });

  collector.start();

  DeviceInfo usb = makeDevice("USB Drive", DeviceClass::Storage, DeviceOrigin::External,
                               "1234", "5678", "", "USB\\VID_1234&PID_5678");
  DeviceInfo bt = makeDevice("BT Headset", DeviceClass::Bluetooth, DeviceOrigin::External,
                              "AAAA", "BBBB", "", "BTH\\RFCOMM\\0000");
  DeviceInfo pcie = makeDevice("GPU", DeviceClass::PCIe, DeviceOrigin::Internal,
                                "8086", "1234", "", "PCI\\VEN_8086&DEV_1234");

  collector.reportConnection(usb);
  collector.reportConnection(bt);
  collector.reportConnection(pcie);

  ASSERT_EQ(count.load(), 3);
  ASSERT_EQ(collector.devicesTracked(), 3u);
  collector.stop();
  PASS();
}

// ==================== Disconnect Tests ====================

static void test_disconnect_detection() {
  TEST("Disconnect: device disconnection detected");
  DeviceCollector collector;
  std::atomic<int> count{0};
  DeviceEventKind capturedKind = DeviceEventKind::Connected;

  collector.setCallback([&](DeviceEventKind kind, DeviceDetectionOrigin origin, const DeviceInfo& info) {
    count++;
    capturedKind = kind;
  });

  collector.start();

  DeviceInfo usb = makeDevice("USB Drive", DeviceClass::Storage, DeviceOrigin::External,
                               "1234", "5678", "", "USB\\VID_1234&PID_5678");
  collector.reportConnection(usb);
  ASSERT_EQ(collector.devicesTracked(), 1u);

  collector.reportDisconnection(usb);
  ASSERT_EQ(count.load(), 2);
  ASSERT_EQ(capturedKind, DeviceEventKind::Disconnected);
  ASSERT_EQ(collector.devicesTracked(), 0u);
  collector.stop();
  PASS();
}

// ==================== Change Tests ====================

static void test_change_detection() {
  TEST("Change: device change detected");
  DeviceCollector collector;
  std::atomic<int> count{0};
  DeviceEventKind capturedKind = DeviceEventKind::Connected;

  collector.setCallback([&](DeviceEventKind kind, DeviceDetectionOrigin origin, const DeviceInfo& info) {
    count++;
    capturedKind = kind;
  });

  collector.start();

  DeviceInfo usb = makeDevice("USB Drive", DeviceClass::Storage, DeviceOrigin::External,
                               "1234", "5678", "", "USB\\VID_1234&PID_5678");
  collector.reportConnection(usb);
  collector.reportChange(usb);

  ASSERT_EQ(count.load(), 2);
  ASSERT_EQ(capturedKind, DeviceEventKind::Changed);
  ASSERT_EQ(collector.devicesTracked(), 1u);
  collector.stop();
  PASS();
}

// ==================== Duplicate Notification Tests ====================

static void test_duplicate_connect_suppressed() {
  TEST("Duplicate: rapid duplicate connect suppressed");
  DeviceCollector collector;
  std::atomic<int> count{0};

  collector.setCallback([&](DeviceEventKind kind, DeviceDetectionOrigin origin, const DeviceInfo& info) {
    count++;
  });

  collector.start();

  DeviceInfo usb = makeDevice("USB Drive", DeviceClass::Storage, DeviceOrigin::External,
                               "1234", "5678", "", "USB\\VID_1234&PID_5678");
  collector.reportConnection(usb);
  collector.reportConnection(usb);
  collector.reportConnection(usb);

  ASSERT_EQ(count.load(), 1);
  collector.stop();
  PASS();
}

static void test_different_devices_not_deduped() {
  TEST("Duplicate: different devices not suppressed");
  DeviceCollector collector;
  std::atomic<int> count{0};

  collector.setCallback([&](DeviceEventKind kind, DeviceDetectionOrigin origin, const DeviceInfo& info) {
    count++;
  });

  collector.start();

  DeviceInfo a = makeDevice("Device A", DeviceClass::Storage, DeviceOrigin::External,
                             "1234", "AAAA", "", "USB\\VID_1234&PID_AAAA");
  DeviceInfo b = makeDevice("Device B", DeviceClass::Storage, DeviceOrigin::External,
                             "5678", "BBBB", "", "USB\\VID_5678&PID_BBBB");
  collector.reportConnection(a);
  collector.reportConnection(b);

  ASSERT_EQ(count.load(), 2);
  collector.stop();
  PASS();
}

// ==================== External/Internal/Virtual Tests ====================

static void test_origin_external() {
  TEST("Origin: USB detected as External");
  DeviceCollector collector;
  DeviceOrigin capturedOrigin = DeviceOrigin::Unknown;

  collector.setCallback([&](DeviceEventKind kind, DeviceDetectionOrigin origin, const DeviceInfo& info) {
    capturedOrigin = info.origin;
  });

  collector.start();
  DeviceInfo usb = makeDevice("USB Drive", DeviceClass::Storage, DeviceOrigin::External);
  collector.reportConnection(usb);

  ASSERT_EQ(capturedOrigin, DeviceOrigin::External);
  collector.stop();
  PASS();
}

static void test_origin_internal() {
  TEST("Origin: PCIe detected as Internal");
  DeviceCollector collector;
  DeviceOrigin capturedOrigin = DeviceOrigin::Unknown;

  collector.setCallback([&](DeviceEventKind kind, DeviceDetectionOrigin origin, const DeviceInfo& info) {
    capturedOrigin = info.origin;
  });

  collector.start();
  DeviceInfo gpu = makeDevice("GPU", DeviceClass::PCIe, DeviceOrigin::Internal);
  collector.reportConnection(gpu);

  ASSERT_EQ(capturedOrigin, DeviceOrigin::Internal);
  collector.stop();
  PASS();
}

static void test_origin_virtual() {
  TEST("Origin: Virtual device detected as Virtual");
  DeviceCollector collector;
  DeviceOrigin capturedOrigin = DeviceOrigin::Unknown;

  collector.setCallback([&](DeviceEventKind kind, DeviceDetectionOrigin origin, const DeviceInfo& info) {
    capturedOrigin = info.origin;
  });

  collector.start();
  DeviceInfo virt = makeDevice("Virtual NIC", DeviceClass::Network, DeviceOrigin::Virtual);
  collector.reportConnection(virt);

  ASSERT_EQ(capturedOrigin, DeviceOrigin::Virtual);
  collector.stop();
  PASS();
}

static void test_origin_unknown() {
  TEST("Origin: Unknown device detected as Unknown");
  DeviceCollector collector;
  DeviceOrigin capturedOrigin = DeviceOrigin::External;

  collector.setCallback([&](DeviceEventKind kind, DeviceDetectionOrigin origin, const DeviceInfo& info) {
    capturedOrigin = info.origin;
  });

  collector.start();
  DeviceInfo unk = makeDevice("Mystery Device", DeviceClass::Other, DeviceOrigin::Unknown);
  collector.reportConnection(unk);

  ASSERT_EQ(capturedOrigin, DeviceOrigin::Unknown);
  collector.stop();
  PASS();
}

// ==================== Unknown Device Tests ====================

static void test_unknown_class_not_tracked() {
  TEST("Unknown: Unknown class devices not tracked");
  DeviceCollector collector;
  std::atomic<int> count{0};

  collector.setCallback([&](DeviceEventKind kind, DeviceDetectionOrigin origin, const DeviceInfo& info) {
    count++;
  });

  collector.start();

  DeviceInfo unk;
  unk.device_class = DeviceClass::Unknown;
  unk.identity.friendly_name = "Mystery";
  unk.timestamp_ms = 1000;
  collector.reportConnection(unk);

  ASSERT_EQ(count.load(), 0);
  ASSERT_EQ(collector.devicesTracked(), 0u);
  collector.stop();
  PASS();
}

// ==================== Rapid Reconnect Tests ====================

static void test_rapid_reconnect() {
  TEST("Rapid reconnect: connect-disconnect-connect tracked correctly");
  DeviceCollector collector;
  std::vector<DeviceEventKind> events;

  collector.setCallback([&](DeviceEventKind kind, DeviceDetectionOrigin origin, const DeviceInfo& info) {
    events.push_back(kind);
  });

  collector.start();

  DeviceInfo usb = makeDevice("USB Drive", DeviceClass::Storage, DeviceOrigin::External,
                               "1234", "5678", "", "USB\\VID_1234&PID_5678");

  collector.reportConnection(usb);
  collector.reportDisconnection(usb);
  collector.reportConnection(usb);

  ASSERT_TRUE(events.size() >= 3);
  ASSERT_EQ(events[0], DeviceEventKind::Connected);
  ASSERT_EQ(events[1], DeviceEventKind::Disconnected);
  ASSERT_EQ(events[2], DeviceEventKind::Connected);
  ASSERT_EQ(collector.devicesTracked(), 1u);
  collector.stop();
  PASS();
}

static void test_rapid_reconnect_dedup() {
  TEST("Rapid reconnect: dedup prevents rapid fire");
  DeviceCollector collector;
  std::atomic<int> count{0};

  collector.setCallback([&](DeviceEventKind kind, DeviceDetectionOrigin origin, const DeviceInfo& info) {
    count++;
  });

  collector.start();

  DeviceInfo usb = makeDevice("USB Drive", DeviceClass::Storage, DeviceOrigin::External,
                               "1234", "5678", "", "USB\\VID_1234&PID_5678");

  for (int i = 0; i < 10; i++) {
    collector.reportConnection(usb);
  }

  ASSERT_EQ(count.load(), 1);
  collector.stop();
  PASS();
}

// ==================== Config Filtering Tests ====================

static void test_config_disable_usb() {
  TEST("Config: disable USB tracking");
  DeviceCollectorConfig cfg = DeviceCollectorConfig::defaults();
  cfg.track_usb = false;

  DeviceCollector collector;
  std::atomic<int> count{0};

  collector.setCallback([&](DeviceEventKind kind, DeviceDetectionOrigin origin, const DeviceInfo& info) {
    count++;
  });

  collector.start(cfg);

  DeviceInfo usb = makeDevice("USB Drive", DeviceClass::USB, DeviceOrigin::External);
  collector.reportConnection(usb);

  ASSERT_EQ(count.load(), 0);
  ASSERT_EQ(collector.devicesTracked(), 0u);
  collector.stop();
  PASS();
}

static void test_config_disable_bluetooth() {
  TEST("Config: disable Bluetooth tracking");
  DeviceCollectorConfig cfg = DeviceCollectorConfig::defaults();
  cfg.track_bluetooth = false;

  DeviceCollector collector;
  std::atomic<int> count{0};

  collector.setCallback([&](DeviceEventKind kind, DeviceDetectionOrigin origin, const DeviceInfo& info) {
    count++;
  });

  collector.start(cfg);

  DeviceInfo bt = makeDevice("BT Mouse", DeviceClass::Bluetooth, DeviceOrigin::External);
  collector.reportConnection(bt);

  ASSERT_EQ(count.load(), 0);
  ASSERT_EQ(collector.devicesTracked(), 0u);
  collector.stop();
  PASS();
}

// ==================== Report When Not Running Tests ====================

static void test_report_when_not_running() {
  TEST("Report when not running: events ignored");
  DeviceCollector collector;
  std::atomic<int> count{0};

  collector.setCallback([&](DeviceEventKind kind, DeviceDetectionOrigin origin, const DeviceInfo& info) {
    count++;
  });

  DeviceInfo usb = makeDevice("USB Drive", DeviceClass::Storage, DeviceOrigin::External);
  collector.reportConnection(usb);
  collector.reportDisconnection(usb);
  collector.reportChange(usb);

  ASSERT_EQ(count.load(), 0);
  PASS();
}

// ==================== Event Log Tests ====================

static void test_event_log_accumulates() {
  TEST("Event log: events accumulate");
  DeviceCollector collector;
  collector.start();

  for (int i = 0; i < 5; i++) {
    DeviceInfo d = makeDevice("Device " + std::to_string(i), DeviceClass::Storage, DeviceOrigin::External,
                               "1234", "5678", "", "USB\\VID_1234&PID_5678_" + std::to_string(i));
    collector.reportConnection(d);
  }

  auto recent = collector.recentEvents(10);
  ASSERT_TRUE(recent.size() == 5);
  ASSERT_EQ(collector.eventsEmitted(), 5u);
  collector.stop();
  PASS();
}

// ==================== Main ====================

#ifndef MONIX_KERNEL_BUILD
int main() {
  printf("=== MONIX Device Collector Tests ===\n\n");

  printf("[DeviceClass Names]\n");
  test_device_class_names();

  printf("[DeviceClassFromName]\n");
  test_class_from_name();

  printf("[DeviceClassFromInstanceId]\n");
  test_class_from_instance_id();

  printf("[DeviceClassFromGuid]\n");
  test_class_from_guid();

  printf("[DeviceOrigin]\n");
  test_origin_names();
  test_detect_origin();

  printf("[DeviceEventKind]\n");
  test_event_kind_names();
  test_event_kind_actions();

  printf("[DetectionOrigin]\n");
  test_detection_origin_names();

  printf("[DeviceIdentity]\n");
  test_device_identity_defaults();
  test_device_identity_has_ids();
  test_device_identity_summary();

  printf("[DeviceInfo]\n");
  test_device_info_defaults();
  test_device_info_is_valid();
  test_device_info_is_same_device();

  printf("[Collector Lifecycle]\n");
  test_collector_start_stop();
  test_collector_double_start();
  test_collector_stop_without_start();
  test_collector_config_defaults();

  printf("[Connect]\n");
  test_connect_detection();
  test_connect_multiple_devices();

  printf("[Disconnect]\n");
  test_disconnect_detection();

  printf("[Change]\n");
  test_change_detection();

  printf("[Duplicate Notification]\n");
  test_duplicate_connect_suppressed();
  test_different_devices_not_deduped();

  printf("[Origin]\n");
  test_origin_external();
  test_origin_internal();
  test_origin_virtual();
  test_origin_unknown();

  printf("[Unknown Device]\n");
  test_unknown_class_not_tracked();

  printf("[Rapid Reconnect]\n");
  test_rapid_reconnect();
  test_rapid_reconnect_dedup();

  printf("[Config Filtering]\n");
  test_config_disable_usb();
  test_config_disable_bluetooth();

  printf("[Report When Not Running]\n");
  test_report_when_not_running();

  printf("[Event Log]\n");
  test_event_log_accumulates();

  printf("\n=== Results: %d passed, %d failed ===\n", gPassed, gFailed);
  return gFailed > 0 ? 1 : 0;
}
#endif

#ifdef MONIX_KERNEL_BUILD
int GetFailedCount_DeviceCollectorTests() { return gFailed; }

struct KTestEntry {
  const char* display_name;
  void (*func)();
};

static const KTestEntry s_ktests[] = {
  {"DeviceClass: names", test_device_class_names},
  {"DeviceClassFromName: all", test_class_from_name},
  {"DeviceClassFromInstanceId: detection", test_class_from_instance_id},
  {"DeviceClassFromGuid: GUIDs", test_class_from_guid},
  {"DeviceOrigin: names", test_origin_names},
  {"DetectDeviceOrigin: detection", test_detect_origin},
  {"DeviceEventKind: names", test_event_kind_names},
  {"DeviceEventKind: actions", test_event_kind_actions},
  {"DeviceDetectionOrigin: names", test_detection_origin_names},
  {"DeviceIdentity: defaults", test_device_identity_defaults},
  {"DeviceIdentity: hasIds", test_device_identity_has_ids},
  {"DeviceIdentity: summary", test_device_identity_summary},
  {"DeviceInfo: defaults", test_device_info_defaults},
  {"DeviceInfo: isValid", test_device_info_is_valid},
  {"DeviceInfo: isSameDevice", test_device_info_is_same_device},
  {"DeviceCollector: start stop", test_collector_start_stop},
  {"DeviceCollector: double start", test_collector_double_start},
  {"DeviceCollector: stop without start", test_collector_stop_without_start},
  {"DeviceCollectorConfig: defaults", test_collector_config_defaults},
  {"Connect: detection", test_connect_detection},
  {"Connect: multiple devices", test_connect_multiple_devices},
  {"Disconnect: detection", test_disconnect_detection},
  {"Change: detection", test_change_detection},
  {"Duplicate: suppressed", test_duplicate_connect_suppressed},
  {"Duplicate: different not deduped", test_different_devices_not_deduped},
  {"Origin: External", test_origin_external},
  {"Origin: Internal", test_origin_internal},
  {"Origin: Virtual", test_origin_virtual},
  {"Origin: Unknown", test_origin_unknown},
  {"Unknown class: not tracked", test_unknown_class_not_tracked},
  {"Rapid reconnect: tracked", test_rapid_reconnect},
  {"Rapid reconnect: dedup", test_rapid_reconnect_dedup},
  {"Config: disable USB", test_config_disable_usb},
  {"Config: disable Bluetooth", test_config_disable_bluetooth},
  {"Report when not running: ignored", test_report_when_not_running},
  {"Event log: accumulates", test_event_log_accumulates},
};

const KTestEntry* GetKTests_DeviceCollector() { return s_ktests; }
std::size_t GetKTestCount_DeviceCollector() { return sizeof(s_ktests) / sizeof(s_ktests[0]); }
#endif
