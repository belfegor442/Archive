#pragma once

#include "DeviceTypes.hpp"

#include <functional>
#include <vector>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <unordered_map>
#include <unordered_set>
#include <chrono>

namespace monix::collectors::device {

using DeviceCallback = std::function<void(
  DeviceEventKind, DeviceDetectionOrigin, const DeviceInfo&)>;

struct DeviceCollectorConfig {
  bool track_usb = true;
  bool track_pcie = true;
  bool track_bluetooth = true;
  bool track_storage = true;
  bool track_display = true;
  bool track_audio = true;
  bool track_network = true;
  bool track_input = true;
  bool track_camera = true;
  bool track_printer = true;
  bool track_other = true;
  std::uint32_t dedup_window_ms = 3000;
  std::size_t max_devices = 500;

  static DeviceCollectorConfig defaults();
};

class DeviceCollector {
public:
  DeviceCollector();
  ~DeviceCollector();

  DeviceCollector(const DeviceCollector&) = delete;
  DeviceCollector& operator=(const DeviceCollector&) = delete;

  bool start(DeviceCollectorConfig config = DeviceCollectorConfig::defaults());
  bool stop();
  bool isRunning() const;

  void setCallback(DeviceCallback callback);

  void reportConnection(const DeviceInfo& info,
                        DeviceDetectionOrigin origin = DeviceDetectionOrigin::Manual);
  void reportDisconnection(const DeviceInfo& info,
                           DeviceDetectionOrigin origin = DeviceDetectionOrigin::Manual);
  void reportChange(const DeviceInfo& info,
                    DeviceDetectionOrigin origin = DeviceDetectionOrigin::Manual);

  std::size_t eventsEmitted() const;
  std::size_t devicesTracked() const;
  std::vector<DeviceInfo> currentDevices() const;
  std::vector<DeviceInfo> recentEvents(std::size_t count = 100) const;

  bool isDeviceConnected(DeviceId id) const;
  bool isDeviceConnected(const std::string& instance_id) const;

private:
  bool shouldTrack(DeviceClass cls) const;
  void emit(DeviceEventKind kind, DeviceDetectionOrigin origin, const DeviceInfo& info);
  bool isDuplicate(const DeviceInfo& info, DeviceEventKind kind);
  DeviceId generateId(const DeviceIdentity& identity) const;

  DeviceCollectorConfig config_;
  DeviceCallback callback_;
  std::unordered_map<DeviceId, DeviceInfo> tracked_;
  std::vector<DeviceInfo> event_log_;
  std::unordered_set<std::string> dedup_keys_;
  mutable std::mutex mu_;
  std::atomic<bool> running_{false};
  std::atomic<std::size_t> events_emitted_{0};
};

}  // namespace monix::collectors::device
