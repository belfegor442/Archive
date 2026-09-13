#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "ConfigTypes.hpp"

namespace monix::collectors::config {

struct ConfigCollectorConfig {
  bool track_security = true;
  bool track_firewall = true;
  bool track_startup = true;
  bool track_services = true;
  bool track_drivers = true;
  bool track_policies = true;
  bool track_system_config = true;
  ConfigChangeSeverity min_severity = ConfigChangeSeverity::Normal;
  bool include_ignored = false;
  std::chrono::milliseconds poll_interval_ms{5000};
  std::chrono::milliseconds dedup_window_ms{2000};
};

class ConfigCollector {
public:
  using Callback = std::function<void(const ConfigChange&, ConfigDetectionOrigin)>;

  explicit ConfigCollector(ConfigCollectorConfig config = {});
  ~ConfigCollector();

  ConfigCollector(const ConfigCollector&) = delete;
  ConfigCollector& operator=(const ConfigCollector&) = delete;

  bool start();
  bool stop();
  bool isRunning() const;

  void setCallback(Callback cb);
  void setConfig(const ConfigCollectorConfig& config);
  const ConfigCollectorConfig& config() const;

  void addScope(const ConfigScope& scope);
  void clearScopes();

  std::vector<ConfigChange> snapshot() const;
  std::size_t changesTracked() const;
  std::size_t eventsEmitted() const;

  bool takeSnapshot();
  bool poll();

private:
  struct ConfigEntry {
    std::string key_path;
    std::string value_name;
    std::string data;
    ConfigArea area;
    ConfigChangeSeverity severity;
  };

  ConfigChangeSeverity classifyChange(const std::string& key_path, const std::string& value_name) const;
  ConfigArea classifyArea(const std::string& key_path) const;
  std::vector<ConfigEntry> enumerateSecurityConfig() const;
  std::vector<ConfigEntry> enumerateFirewallConfig() const;
  std::vector<ConfigEntry> enumerateStartupConfig() const;
  std::vector<ConfigEntry> enumerateServicesConfig() const;
  std::vector<ConfigEntry> enumerateDriversConfig() const;
  std::vector<ConfigEntry> enumeratePoliciesConfig() const;
  std::vector<ConfigEntry> enumerateSystemConfig() const;

  std::string readRegString(HKEY key, const wchar_t* value_name) const;

  bool isDuplicate(const std::string& key);
  void emit(const ConfigChange& change, ConfigDetectionOrigin origin);
  bool passesScope(const ConfigChange& change) const;

  ConfigCollectorConfig config_;
  Callback callback_;
  mutable std::mutex mu_;
  std::atomic<bool> running_{false};
  std::unordered_map<std::string, ConfigEntry> tracked_;
  std::unordered_set<std::string> dedup_keys_;
  std::vector<ConfigScope> scopes_;
  std::atomic<std::size_t> events_emitted_{0};
};

}  // namespace monix::collectors::config
