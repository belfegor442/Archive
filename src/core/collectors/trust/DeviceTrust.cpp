#include "DeviceTrust.hpp"

#include <algorithm>
#include <chrono>

namespace monix::collectors::trust {

const char* TrustLevelName(TrustLevel l) {
  switch (l) {
    case TrustLevel::Unknown:  return "Unknown";
    case TrustLevel::Local:    return "Local";
    case TrustLevel::Trusted:  return "Trusted";
    case TrustLevel::Verified: return "Verified";
  }
  return "Unknown";
}

TrustLevel TrustLevelFromName(const std::string& name) {
  if (name == "Unknown")  return TrustLevel::Unknown;
  if (name == "Local")    return TrustLevel::Local;
  if (name == "Trusted")  return TrustLevel::Trusted;
  if (name == "Verified") return TrustLevel::Verified;
  return TrustLevel::Unknown;
}

bool DeviceTrust::isValid() const {
  return !device_id.empty();
}

bool DeviceTrust::isAtLeast(TrustLevel min_level) const {
  return static_cast<std::uint8_t>(level) >= static_cast<std::uint8_t>(min_level);
}

std::string DeviceTrust::summary() const {
  std::string result = device_id;
  if (!device_name.empty()) result += " (" + device_name + ")";
  result += " " + std::string(TrustLevelName(level));
  if (!verifier.empty()) result += " by " + verifier;
  return result;
}

bool TrustChangeEvent::isValid() const {
  return !device_id.empty();
}

std::string TrustChangeEvent::summary() const {
  return device_id + " " + std::string(TrustLevelName(old_level)) +
    " → " + std::string(TrustLevelName(new_level));
}

DeviceTrustManager::DeviceTrustManager() {}
DeviceTrustManager::~DeviceTrustManager() {}

void DeviceTrustManager::setCallback(TrustCallback cb) {
  std::lock_guard<std::mutex> lock(mu_);
  callback_ = std::move(cb);
}

void DeviceTrustManager::registerDevice(const std::string& device_id, TrustLevel level,
    const std::string& source, const std::string& verifier) {
  std::lock_guard<std::mutex> lock(mu_);
  DeviceTrust trust;
  trust.device_id = device_id;
  trust.level = level;
  trust.source = source;
  trust.verifier = verifier;
  trust.established_at_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::system_clock::now().time_since_epoch()).count();
  devices_[device_id] = trust;
}

bool DeviceTrustManager::setTrustLevel(const std::string& device_id, TrustLevel new_level,
    const std::string& reason) {
  TrustChangeEvent event;
  {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = devices_.find(device_id);
    if (it == devices_.end()) return false;

    event.old_level = it->second.level;
    event.new_level = new_level;
    event.device_id = device_id;
    event.reason = reason;
    event.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count();

    it->second.level = new_level;
    it->second.last_verified_ms = event.timestamp_ms;
  }

  events_emitted_++;
  if (callback_) {
    callback_(event);
  }
  return true;
}

DeviceTrust DeviceTrustManager::getDevice(const std::string& device_id) const {
  std::lock_guard<std::mutex> lock(mu_);
  auto it = devices_.find(device_id);
  if (it == devices_.end()) return DeviceTrust{};
  return it->second;
}

std::vector<DeviceTrust> DeviceTrustManager::allDevices() const {
  std::lock_guard<std::mutex> lock(mu_);
  std::vector<DeviceTrust> result;
  for (const auto& [id, trust] : devices_) {
    result.push_back(trust);
  }
  return result;
}

std::vector<DeviceTrust> DeviceTrustManager::byLevel(TrustLevel level) const {
  std::lock_guard<std::mutex> lock(mu_);
  std::vector<DeviceTrust> result;
  for (const auto& [id, trust] : devices_) {
    if (trust.level == level) result.push_back(trust);
  }
  return result;
}

bool DeviceTrustManager::hasDevice(const std::string& device_id) const {
  std::lock_guard<std::mutex> lock(mu_);
  return devices_.count(device_id) > 0;
}

TrustLevel DeviceTrustManager::getLevel(const std::string& device_id) const {
  std::lock_guard<std::mutex> lock(mu_);
  auto it = devices_.find(device_id);
  if (it == devices_.end()) return TrustLevel::Unknown;
  return it->second.level;
}

bool DeviceTrustManager::isTrusted(const std::string& device_id) const {
  return getLevel(device_id) >= TrustLevel::Trusted;
}

bool DeviceTrustManager::isVerified(const std::string& device_id) const {
  return getLevel(device_id) >= TrustLevel::Verified;
}

std::size_t DeviceTrustManager::deviceCount() const {
  std::lock_guard<std::mutex> lock(mu_);
  return devices_.size();
}

std::size_t DeviceTrustManager::eventsEmitted() const {
  return events_emitted_.load();
}

void DeviceTrustManager::clear() {
  std::lock_guard<std::mutex> lock(mu_);
  devices_.clear();
}

}  // namespace monix::collectors::trust
