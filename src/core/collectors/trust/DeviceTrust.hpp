#pragma once

#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace monix::collectors::trust {

enum class TrustLevel : std::uint8_t {
  Unknown = 0,
  Local = 1,
  Trusted = 2,
  Verified = 3
};

const char* TrustLevelName(TrustLevel l);
TrustLevel TrustLevelFromName(const std::string& name);

struct DeviceTrust {
  std::string device_id;
  std::string device_name;
  std::string source;
  TrustLevel level = TrustLevel::Unknown;
  std::string verifier;
  std::int64_t established_at_ms = 0;
  std::int64_t last_verified_ms = 0;
  std::string notes;

  bool isValid() const;
  bool isAtLeast(TrustLevel min_level) const;
  std::string summary() const;
};

struct TrustChangeEvent {
  std::string device_id;
  TrustLevel old_level = TrustLevel::Unknown;
  TrustLevel new_level = TrustLevel::Unknown;
  std::string reason;
  std::int64_t timestamp_ms = 0;

  bool isValid() const;
  std::string summary() const;
};

using TrustCallback = std::function<void(const TrustChangeEvent&)>;

class DeviceTrustManager {
public:
  DeviceTrustManager();
  ~DeviceTrustManager();

  DeviceTrustManager(const DeviceTrustManager&) = delete;
  DeviceTrustManager& operator=(const DeviceTrustManager&) = delete;

  void setCallback(TrustCallback cb);

  void registerDevice(const std::string& device_id, TrustLevel level,
    const std::string& source = "", const std::string& verifier = "");

  bool setTrustLevel(const std::string& device_id, TrustLevel new_level,
    const std::string& reason = "");

  DeviceTrust getDevice(const std::string& device_id) const;
  std::vector<DeviceTrust> allDevices() const;
  std::vector<DeviceTrust> byLevel(TrustLevel level) const;

  bool hasDevice(const std::string& device_id) const;
  TrustLevel getLevel(const std::string& device_id) const;
  bool isTrusted(const std::string& device_id) const;
  bool isVerified(const std::string& device_id) const;

  std::size_t deviceCount() const;
  std::size_t eventsEmitted() const;

  void clear();

private:
  TrustCallback callback_;
  mutable std::mutex mu_;
  std::unordered_map<std::string, DeviceTrust> devices_;
  std::atomic<std::size_t> events_emitted_{0};
};

}  // namespace monix::collectors::trust
