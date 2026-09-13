#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <optional>

namespace monix::collectors::device {

using DeviceId = std::uint64_t;

enum class DeviceClass : std::uint8_t {
  USB,
  PCIe,
  Bluetooth,
  Storage,
  Display,
  Audio,
  Network,
  Input,
  Camera,
  Printer,
  Other,
  Unknown
};

const char* DeviceClassName(DeviceClass c);
DeviceClass DeviceClassFromName(const std::string& name);
DeviceClass DeviceClassFromInstanceId(const std::string& instance_id);
DeviceClass DeviceClassFromGuid(const std::string& guid);

enum class DeviceOrigin : std::uint8_t {
  Internal,
  External,
  Virtual,
  Unknown
};

const char* DeviceOriginName(DeviceOrigin o);
DeviceOrigin DetectDeviceOrigin(const std::string& instance_id, const std::string& bus);

enum class DeviceEventKind : std::uint8_t {
  Connected,
  Disconnected,
  Changed,
  Available,
  Unavailable
};

const char* DeviceEventKindName(DeviceEventKind k);
std::string DeviceEventKindAction(DeviceEventKind k);

struct DeviceIdentity {
  std::string vendor_id;
  std::string product_id;
  std::string serial;
  std::string bus;
  std::string instance_id;
  std::string device_class;
  std::string friendly_name;
  std::string description;

  bool hasIds() const;
  bool hasInstanceId() const;
  std::string summary() const;
};

struct DeviceInfo {
  DeviceId id = 0;
  DeviceIdentity identity;
  DeviceClass device_class = DeviceClass::Unknown;
  DeviceOrigin origin = DeviceOrigin::Unknown;
  bool connected = false;
  std::int64_t timestamp_ms = 0;

  bool isValid() const;
  bool isSameDevice(const DeviceInfo& other) const;
};

enum class DeviceDetectionOrigin : std::uint8_t {
  WMI,
  SetupAPI,
  Manual,
  Polling
};

const char* DeviceDetectionOriginName(DeviceDetectionOrigin o);

}  // namespace monix::collectors::device
