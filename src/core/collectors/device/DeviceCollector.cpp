#include "DeviceCollector.hpp"

#include <algorithm>
#include <chrono>

namespace monix::collectors::device {

DeviceCollectorConfig DeviceCollectorConfig::defaults() {
  DeviceCollectorConfig cfg;
  cfg.track_usb = true;
  cfg.track_pcie = true;
  cfg.track_bluetooth = true;
  cfg.track_storage = true;
  cfg.track_display = true;
  cfg.track_audio = true;
  cfg.track_network = true;
  cfg.track_input = true;
  cfg.track_camera = true;
  cfg.track_printer = true;
  cfg.track_other = true;
  cfg.dedup_window_ms = 3000;
  cfg.max_devices = 500;
  return cfg;
}

DeviceCollector::DeviceCollector() = default;

DeviceCollector::~DeviceCollector() {
  stop();
}

bool DeviceCollector::start(DeviceCollectorConfig config) {
  if (running_) return false;
  config_ = std::move(config);
  running_ = true;
  events_emitted_ = 0;
  tracked_.clear();
  event_log_.clear();
  dedup_keys_.clear();
  return true;
}

bool DeviceCollector::stop() {
  if (!running_) return false;
  running_ = false;
  tracked_.clear();
  event_log_.clear();
  dedup_keys_.clear();
  return true;
}

bool DeviceCollector::isRunning() const {
  return running_;
}

void DeviceCollector::setCallback(DeviceCallback callback) {
  callback_ = std::move(callback);
}

void DeviceCollector::reportConnection(const DeviceInfo& info, DeviceDetectionOrigin origin) {
  if (!running_) return;
  if (!shouldTrack(info.device_class)) return;
  if (isDuplicate(info, DeviceEventKind::Connected)) return;

  DeviceInfo enriched = info;
  enriched.connected = true;
  if (enriched.timestamp_ms == 0) {
    enriched.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count();
  }
  if (enriched.id == 0) {
    enriched.id = generateId(enriched.identity);
  }

  {
    std::lock_guard<std::mutex> lock(mu_);
    if (tracked_.size() < config_.max_devices) {
      tracked_[enriched.id] = enriched;
    }
  }

  emit(DeviceEventKind::Connected, origin, enriched);
}

void DeviceCollector::reportDisconnection(const DeviceInfo& info, DeviceDetectionOrigin origin) {
  if (!running_) return;
  if (!shouldTrack(info.device_class)) return;
  if (isDuplicate(info, DeviceEventKind::Disconnected)) return;

  DeviceInfo enriched = info;
  enriched.connected = false;
  if (enriched.timestamp_ms == 0) {
    enriched.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count();
  }
  if (enriched.id == 0) {
    enriched.id = generateId(enriched.identity);
  }

  {
    std::lock_guard<std::mutex> lock(mu_);
    tracked_.erase(enriched.id);
  }

  emit(DeviceEventKind::Disconnected, origin, enriched);
}

void DeviceCollector::reportChange(const DeviceInfo& info, DeviceDetectionOrigin origin) {
  if (!running_) return;
  if (!shouldTrack(info.device_class)) return;
  if (isDuplicate(info, DeviceEventKind::Changed)) return;

  DeviceInfo enriched = info;
  enriched.connected = true;
  if (enriched.timestamp_ms == 0) {
    enriched.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count();
  }
  if (enriched.id == 0) {
    enriched.id = generateId(enriched.identity);
  }

  {
    std::lock_guard<std::mutex> lock(mu_);
    tracked_[enriched.id] = enriched;
  }

  emit(DeviceEventKind::Changed, origin, enriched);
}

std::size_t DeviceCollector::eventsEmitted() const {
  return events_emitted_;
}

std::size_t DeviceCollector::devicesTracked() const {
  return tracked_.size();
}

std::vector<DeviceInfo> DeviceCollector::currentDevices() const {
  std::lock_guard<std::mutex> lock(mu_);
  std::vector<DeviceInfo> result;
  for (const auto& [id, info] : tracked_) {
    result.push_back(info);
  }
  return result;
}

std::vector<DeviceInfo> DeviceCollector::recentEvents(std::size_t count) const {
  std::lock_guard<std::mutex> lock(mu_);
  std::size_t start = event_log_.size() > count ? event_log_.size() - count : 0;
  return std::vector<DeviceInfo>(event_log_.begin() + start, event_log_.end());
}

bool DeviceCollector::isDeviceConnected(DeviceId id) const {
  std::lock_guard<std::mutex> lock(mu_);
  auto it = tracked_.find(id);
  return it != tracked_.end() && it->second.connected;
}

bool DeviceCollector::isDeviceConnected(const std::string& instance_id) const {
  std::lock_guard<std::mutex> lock(mu_);
  for (const auto& [id, info] : tracked_) {
    if (info.identity.instance_id == instance_id && info.connected) {
      return true;
    }
  }
  return false;
}

bool DeviceCollector::shouldTrack(DeviceClass cls) const {
  switch (cls) {
    case DeviceClass::USB:       return config_.track_usb;
    case DeviceClass::PCIe:      return config_.track_pcie;
    case DeviceClass::Bluetooth: return config_.track_bluetooth;
    case DeviceClass::Storage:   return config_.track_storage;
    case DeviceClass::Display:   return config_.track_display;
    case DeviceClass::Audio:     return config_.track_audio;
    case DeviceClass::Network:   return config_.track_network;
    case DeviceClass::Input:     return config_.track_input;
    case DeviceClass::Camera:    return config_.track_camera;
    case DeviceClass::Printer:   return config_.track_printer;
    case DeviceClass::Other:     return config_.track_other;
    case DeviceClass::Unknown:   return false;
  }
  return false;
}

void DeviceCollector::emit(DeviceEventKind kind, DeviceDetectionOrigin origin, const DeviceInfo& info) {
  events_emitted_++;
  {
    std::lock_guard<std::mutex> lock(mu_);
    if (event_log_.size() >= config_.max_devices) {
      event_log_.erase(event_log_.begin());
    }
    event_log_.push_back(info);
  }
  if (callback_) {
    callback_(kind, origin, info);
  }
}

bool DeviceCollector::isDuplicate(const DeviceInfo& info, DeviceEventKind kind) {
  std::string key;
  key.reserve(128);
  key += std::to_string(static_cast<int>(kind));
  key += ":";
  if (!info.identity.instance_id.empty()) {
    key += info.identity.instance_id;
  } else if (info.id != 0) {
    key += std::to_string(info.id);
  } else {
    return false;
  }

  auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::system_clock::now().time_since_epoch()).count();

  std::string dedup_key = key + "@" + std::to_string(now / config_.dedup_window_ms);
  std::lock_guard<std::mutex> lock(mu_);

  if (kind == DeviceEventKind::Disconnected || kind == DeviceEventKind::Unavailable) {
    std::string deviceId = key.substr(key.find(':') + 1);
    std::string prefixConn = std::to_string(static_cast<int>(DeviceEventKind::Connected)) + ":" + deviceId;
    std::string prefixAvail = std::to_string(static_cast<int>(DeviceEventKind::Available)) + ":" + deviceId;
    for (auto it = dedup_keys_.begin(); it != dedup_keys_.end(); ) {
      if (it->compare(0, prefixConn.size(), prefixConn) == 0 ||
          it->compare(0, prefixAvail.size(), prefixAvail) == 0) {
        it = dedup_keys_.erase(it);
      } else {
        ++it;
      }
    }
    return false;
  }

  if (dedup_keys_.count(dedup_key) > 0) {
    return true;
  }
  dedup_keys_.insert(dedup_key);
  return false;
}

DeviceId DeviceCollector::generateId(const DeviceIdentity& identity) const {
  if (!identity.instance_id.empty()) {
    std::hash<std::string> hasher;
    return static_cast<DeviceId>(hasher(identity.instance_id));
  }
  if (!identity.vendor_id.empty() && !identity.product_id.empty()) {
    std::hash<std::string> hasher;
    DeviceId vid = hasher(identity.vendor_id);
    DeviceId pid = hasher(identity.product_id);
    return vid ^ (pid << 1);
  }
  return 0;
}

}  // namespace monix::collectors::device
