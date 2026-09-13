#include "ConfigCollector.hpp"

#include <algorithm>
#include <sstream>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#pragma comment(lib, "advapi32.lib")

namespace monix::collectors::config {

ConfigCollector::ConfigCollector(ConfigCollectorConfig config)
    : config_(std::move(config)) {}

ConfigCollector::~ConfigCollector() {
  if (running_.load()) {
    stop();
  }
}

bool ConfigCollector::start() {
  bool expected = false;
  if (!running_.compare_exchange_strong(expected, true)) {
    return false;
  }
  takeSnapshot();
  return true;
}

bool ConfigCollector::stop() {
  bool expected = true;
  if (!running_.compare_exchange_strong(expected, false)) {
    return false;
  }
  return true;
}

bool ConfigCollector::isRunning() const {
  return running_.load();
}

void ConfigCollector::setCallback(Callback cb) {
  std::lock_guard<std::mutex> lock(mu_);
  callback_ = std::move(cb);
}

void ConfigCollector::setConfig(const ConfigCollectorConfig& config) {
  std::lock_guard<std::mutex> lock(mu_);
  config_ = config;
}

const ConfigCollectorConfig& ConfigCollector::config() const {
  return config_;
}

void ConfigCollector::addScope(const ConfigScope& scope) {
  std::lock_guard<std::mutex> lock(mu_);
  scopes_.push_back(scope);
}

void ConfigCollector::clearScopes() {
  std::lock_guard<std::mutex> lock(mu_);
  scopes_.clear();
}

std::vector<ConfigChange> ConfigCollector::snapshot() const {
  std::lock_guard<std::mutex> lock(mu_);
  std::vector<ConfigChange> result;
  result.reserve(tracked_.size());
  for (const auto& [key, entry] : tracked_) {
    ConfigChange change;
    change.id = std::hash<std::string>{}(key);
    change.area = entry.area;
    change.kind = ConfigChangeKind::Modified;
    change.severity = entry.severity;
    change.key_path = entry.key_path;
    change.value_name = entry.value_name;
    change.new_value = entry.data;
    change.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count();
    result.push_back(std::move(change));
  }
  return result;
}

std::size_t ConfigCollector::changesTracked() const {
  std::lock_guard<std::mutex> lock(mu_);
  return tracked_.size();
}

std::size_t ConfigCollector::eventsEmitted() const {
  return events_emitted_.load();
}

std::string ConfigCollector::readRegString(HKEY key, const wchar_t* value_name) const {
  DWORD type = 0;
  DWORD size = 0;
  if (RegQueryValueExW(key, value_name, nullptr, &type, nullptr, &size) != ERROR_SUCCESS || type != REG_SZ || size == 0) {
    return "";
  }
  std::vector<wchar_t> buf(size / sizeof(wchar_t) + 1, 0);
  if (RegQueryValueExW(key, value_name, nullptr, nullptr, reinterpret_cast<LPBYTE>(buf.data()), &size) != ERROR_SUCCESS) {
    return "";
  }
  int sz = WideCharToMultiByte(CP_UTF8, 0, buf.data(), -1, nullptr, 0, nullptr, nullptr);
  if (sz <= 0) return "";
  std::string result(static_cast<std::size_t>(sz) - 1, '\0');
  WideCharToMultiByte(CP_UTF8, 0, buf.data(), -1, result.data(), sz, nullptr, nullptr);
  return result;
}

ConfigChangeSeverity ConfigCollector::classifyChange(const std::string& key_path, const std::string& value_name) const {
  std::string lower_key = key_path;
  std::transform(lower_key.begin(), lower_key.end(), lower_key.begin(), ::tolower);
  std::string lower_val = value_name;
  std::transform(lower_val.begin(), lower_val.end(), lower_val.begin(), ::tolower);

  if (lower_key.find("lsa") != std::string::npos) return ConfigChangeSeverity::Critical;
  if (lower_key.find("security") != std::string::npos && lower_key.find("policy") != std::string::npos) return ConfigChangeSeverity::Critical;
  if (lower_key.find("firewallpolicy") != std::string::npos) return ConfigChangeSeverity::Critical;
  if (lower_key.find("policies") != std::string::npos) return ConfigChangeSeverity::Important;

  if (lower_val == "start" || lower_val == "type") return ConfigChangeSeverity::Important;
  if (lower_key.find("currentcontrolset\\services") != std::string::npos) return ConfigChangeSeverity::Important;

  if (lower_key.find("run") != std::string::npos) return ConfigChangeSeverity::Important;
  if (lower_key.find("startup") != std::string::npos) return ConfigChangeSeverity::Important;

  return ConfigChangeSeverity::Normal;
}

ConfigArea ConfigCollector::classifyArea(const std::string& key_path) const {
  std::string lower = key_path;
  std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

  if (lower.find("lsa") != std::string::npos || lower.find("security") != std::string::npos) {
    return ConfigArea::Security;
  }
  if (lower.find("firewall") != std::string::npos) {
    return ConfigArea::Firewall;
  }
  if (lower.find("run") != std::string::npos || lower.find("startup") != std::string::npos) {
    return ConfigArea::Startup;
  }
  if (lower.find("currentcontrolset\\services") != std::string::npos) {
    return ConfigArea::Services;
  }
  if (lower.find("drivers") != std::string::npos) {
    return ConfigArea::Drivers;
  }
  if (lower.find("policies") != std::string::npos) {
    return ConfigArea::Policies;
  }
  return ConfigArea::SystemConfiguration;
}

std::vector<ConfigCollector::ConfigEntry> ConfigCollector::enumerateSecurityConfig() const {
  std::vector<ConfigEntry> result;
  const wchar_t* paths[] = {
    L"SYSTEM\\CurrentControlSet\\Control\\Lsa",
    L"SYSTEM\\CurrentControlSet\\Control\\Lsa\\Data",
    L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Winlogon"
  };
  for (const auto& path : paths) {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, path, 0, KEY_READ, &key) != ERROR_SUCCESS) continue;
    DWORD value_count = 0;
    RegQueryInfoKeyW(key, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, &value_count, nullptr, nullptr, nullptr, nullptr);
    for (DWORD i = 0; i < value_count; i++) {
      wchar_t val_name[256]{}; DWORD name_len = 256;
      if (RegEnumValueW(key, i, val_name, &name_len, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS) continue;
      std::string vname;
      { int sz = WideCharToMultiByte(CP_UTF8, 0, val_name, -1, nullptr, 0, nullptr, nullptr);
        if (sz > 0) { vname.resize(sz - 1); WideCharToMultiByte(CP_UTF8, 0, val_name, -1, vname.data(), sz, nullptr, nullptr); } }
      std::string kpath;
      { int sz = WideCharToMultiByte(CP_UTF8, 0, path, -1, nullptr, 0, nullptr, nullptr);
        if (sz > 0) { kpath.resize(sz - 1); WideCharToMultiByte(CP_UTF8, 0, path, -1, kpath.data(), sz, nullptr, nullptr); } }
      std::string data = readRegString(key, val_name);
      ConfigEntry entry;
      entry.key_path = kpath;
      entry.value_name = vname;
      entry.data = data;
      entry.area = ConfigArea::Security;
      entry.severity = classifyChange(kpath, vname);
      result.push_back(std::move(entry));
    }
    RegCloseKey(key);
  }
  return result;
}

std::vector<ConfigCollector::ConfigEntry> ConfigCollector::enumerateFirewallConfig() const {
  std::vector<ConfigEntry> result;
  HKEY key = nullptr;
  if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services\\SharedAccess\\Parameters\\FirewallPolicy", 0, KEY_READ, &key) != ERROR_SUCCESS) {
    return result;
  }
  DWORD value_count = 0;
  RegQueryInfoKeyW(key, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, &value_count, nullptr, nullptr, nullptr, nullptr);
  for (DWORD i = 0; i < value_count; i++) {
    wchar_t val_name[256]{}; DWORD name_len = 256;
    if (RegEnumValueW(key, i, val_name, &name_len, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS) continue;
    std::string vname;
    { int sz = WideCharToMultiByte(CP_UTF8, 0, val_name, -1, nullptr, 0, nullptr, nullptr);
      if (sz > 0) { vname.resize(sz - 1); WideCharToMultiByte(CP_UTF8, 0, val_name, -1, vname.data(), sz, nullptr, nullptr); } }
    std::string data = readRegString(key, val_name);
    ConfigEntry entry;
    entry.key_path = "SYSTEM\\CurrentControlSet\\Services\\SharedAccess\\Parameters\\FirewallPolicy";
    entry.value_name = vname;
    entry.data = data;
    entry.area = ConfigArea::Firewall;
    entry.severity = ConfigChangeSeverity::Critical;
    result.push_back(std::move(entry));
  }
  RegCloseKey(key);
  return result;
}

std::vector<ConfigCollector::ConfigEntry> ConfigCollector::enumerateStartupConfig() const {
  std::vector<ConfigEntry> result;
  const wchar_t* paths[] = {
    L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run",
    L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\RunOnce"
  };
  for (const auto& path : paths) {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, path, 0, KEY_READ, &key) != ERROR_SUCCESS) continue;
    DWORD value_count = 0;
    RegQueryInfoKeyW(key, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, &value_count, nullptr, nullptr, nullptr, nullptr);
    for (DWORD i = 0; i < value_count; i++) {
      wchar_t val_name[256]{}; DWORD name_len = 256;
      if (RegEnumValueW(key, i, val_name, &name_len, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS) continue;
      std::string vname;
      { int sz = WideCharToMultiByte(CP_UTF8, 0, val_name, -1, nullptr, 0, nullptr, nullptr);
        if (sz > 0) { vname.resize(sz - 1); WideCharToMultiByte(CP_UTF8, 0, val_name, -1, vname.data(), sz, nullptr, nullptr); } }
      std::string data = readRegString(key, val_name);
      ConfigEntry entry;
      entry.key_path = vname;
      entry.value_name = vname;
      entry.data = data;
      entry.area = ConfigArea::Startup;
      entry.severity = ConfigChangeSeverity::Important;
      result.push_back(std::move(entry));
    }
    RegCloseKey(key);
  }
  return result;
}

std::vector<ConfigCollector::ConfigEntry> ConfigCollector::enumerateServicesConfig() const {
  std::vector<ConfigEntry> result;
  HKEY sk = nullptr;
  if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services", 0, KEY_READ, &sk) != ERROR_SUCCESS) return result;
  DWORD subkey_count = 0;
  RegQueryInfoKeyW(sk, nullptr, nullptr, nullptr, &subkey_count, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
  DWORD limit = std::min(subkey_count, static_cast<DWORD>(50));
  for (DWORD i = 0; i < limit; i++) {
    wchar_t name[256]{}; DWORD len = 256;
    if (RegEnumKeyExW(sk, i, name, &len, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS) continue;
    HKEY ck = nullptr;
    if (RegOpenKeyExW(sk, name, 0, KEY_READ, &ck) != ERROR_SUCCESS) continue;
    DWORD svc_type = 0, sz = sizeof(svc_type);
    RegQueryValueExW(ck, L"Type", nullptr, nullptr, reinterpret_cast<LPBYTE>(&svc_type), &sz);
    RegCloseKey(ck);
    if ((svc_type & 0x1) == 0) continue;
    std::string kpath;
    { int sz2 = WideCharToMultiByte(CP_UTF8, 0, name, -1, nullptr, 0, nullptr, nullptr);
      if (sz2 > 0) { kpath.resize(sz2 - 1); WideCharToMultiByte(CP_UTF8, 0, name, -1, kpath.data(), sz2, nullptr, nullptr); } }
    ConfigEntry entry;
    entry.key_path = "Services\\" + kpath;
    entry.value_name = "Type";
    entry.data = std::to_string(svc_type);
    entry.area = ConfigArea::Services;
    entry.severity = ConfigChangeSeverity::Important;
    result.push_back(std::move(entry));
  }
  RegCloseKey(sk);
  return result;
}

std::vector<ConfigCollector::ConfigEntry> ConfigCollector::enumerateDriversConfig() const {
  return enumerateServicesConfig();
}

std::vector<ConfigCollector::ConfigEntry> ConfigCollector::enumeratePoliciesConfig() const {
  std::vector<ConfigEntry> result;
  HKEY key = nullptr;
  if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Policies", 0, KEY_READ, &key) != ERROR_SUCCESS) return result;
  DWORD subkey_count = 0;
  RegQueryInfoKeyW(key, nullptr, nullptr, nullptr, &subkey_count, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
  DWORD limit = std::min(subkey_count, static_cast<DWORD>(20));
  for (DWORD i = 0; i < limit; i++) {
    wchar_t name[256]{}; DWORD len = 256;
    if (RegEnumKeyExW(key, i, name, &len, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS) continue;
    std::string kpath;
    { int sz = WideCharToMultiByte(CP_UTF8, 0, name, -1, nullptr, 0, nullptr, nullptr);
      if (sz > 0) { kpath.resize(sz - 1); WideCharToMultiByte(CP_UTF8, 0, name, -1, kpath.data(), sz, nullptr, nullptr); } }
    ConfigEntry entry;
    entry.key_path = "Policies\\" + kpath;
    entry.value_name = "";
    entry.data = "";
    entry.area = ConfigArea::Policies;
    entry.severity = ConfigChangeSeverity::Important;
    result.push_back(std::move(entry));
  }
  RegCloseKey(key);
  return result;
}

std::vector<ConfigCollector::ConfigEntry> ConfigCollector::enumerateSystemConfig() const {
  std::vector<ConfigEntry> result;
  HKEY key = nullptr;
  if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion", 0, KEY_READ, &key) != ERROR_SUCCESS) return result;
  const wchar_t* values[] = {L"ProductName", L"CurrentVersion", L"BuildBranch"};
  for (const auto& vname : values) {
    std::string data = readRegString(key, vname);
    if (data.empty()) continue;
    std::string vn;
    { int sz = WideCharToMultiByte(CP_UTF8, 0, vname, -1, nullptr, 0, nullptr, nullptr);
      if (sz > 0) { vn.resize(sz - 1); WideCharToMultiByte(CP_UTF8, 0, vname, -1, vn.data(), sz, nullptr, nullptr); } }
    ConfigEntry entry;
    entry.key_path = "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion";
    entry.value_name = vn;
    entry.data = data;
    entry.area = ConfigArea::SystemConfiguration;
    entry.severity = ConfigChangeSeverity::Normal;
    result.push_back(std::move(entry));
  }
  RegCloseKey(key);
  return result;
}

bool ConfigCollector::takeSnapshot() {
  std::vector<ConfigEntry> current_entries;
  if (config_.track_security) { auto e = enumerateSecurityConfig(); current_entries.insert(current_entries.end(), e.begin(), e.end()); }
  if (config_.track_firewall) { auto e = enumerateFirewallConfig(); current_entries.insert(current_entries.end(), e.begin(), e.end()); }
  if (config_.track_startup) { auto e = enumerateStartupConfig(); current_entries.insert(current_entries.end(), e.begin(), e.end()); }
  if (config_.track_services) { auto e = enumerateServicesConfig(); current_entries.insert(current_entries.end(), e.begin(), e.end()); }
  if (config_.track_policies) { auto e = enumeratePoliciesConfig(); current_entries.insert(current_entries.end(), e.begin(), e.end()); }
  if (config_.track_system_config) { auto e = enumerateSystemConfig(); current_entries.insert(current_entries.end(), e.begin(), e.end()); }

  std::unordered_set<std::string> current_keys;
  std::vector<std::pair<ConfigChange, ConfigDetectionOrigin>> pending;

  for (const auto& entry : current_entries) {
    std::string key = entry.key_path + "\\" + entry.value_name;
    current_keys.insert(key);

    std::lock_guard<std::mutex> lock(mu_);
    auto it = tracked_.find(key);
    if (it == tracked_.end()) {
      tracked_[key] = entry;
      ConfigChange change;
      change.id = std::hash<std::string>{}(key);
      change.area = entry.area;
      change.kind = ConfigChangeKind::Added;
      change.severity = entry.severity;
      change.key_path = entry.key_path;
      change.value_name = entry.value_name;
      change.new_value = entry.data;
      change.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
      pending.push_back({change, ConfigDetectionOrigin::Registry});
    } else {
      if (it->second.data != entry.data) {
        ConfigChange change;
        change.id = std::hash<std::string>{}(key);
        change.area = entry.area;
        change.kind = ConfigChangeKind::Modified;
        change.severity = entry.severity;
        change.key_path = entry.key_path;
        change.value_name = entry.value_name;
        change.old_value = it->second.data;
        change.new_value = entry.data;
        change.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::system_clock::now().time_since_epoch()).count();
        it->second = entry;
        pending.push_back({change, ConfigDetectionOrigin::Registry});
      }
    }
  }

  {
    std::lock_guard<std::mutex> lock(mu_);
    std::vector<std::string> to_remove;
    for (const auto& [key, entry] : tracked_) {
      if (current_keys.find(key) == current_keys.end()) {
        to_remove.push_back(key);
        ConfigChange change;
        change.id = std::hash<std::string>{}(key);
        change.area = entry.area;
        change.kind = ConfigChangeKind::Removed;
        change.severity = entry.severity;
        change.key_path = entry.key_path;
        change.value_name = entry.value_name;
        change.old_value = entry.data;
        change.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::system_clock::now().time_since_epoch()).count();
        pending.push_back({change, ConfigDetectionOrigin::Registry});
      }
    }
    for (const auto& key : to_remove) {
      tracked_.erase(key);
    }
  }

  for (auto& [change, origin] : pending) {
    if (passesScope(change)) {
      emit(change, origin);
    }
  }

  return true;
}

bool ConfigCollector::poll() {
  if (!running_.load()) return false;
  return takeSnapshot();
}

bool ConfigCollector::isDuplicate(const std::string& key) {
  auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::system_clock::now().time_since_epoch()).count();
  std::string dedup_key = key + "@" + std::to_string(now / config_.dedup_window_ms.count());
  std::lock_guard<std::mutex> lock(mu_);
  if (dedup_keys_.count(dedup_key) > 0) return true;
  dedup_keys_.insert(dedup_key);
  return false;
}

bool ConfigCollector::passesScope(const ConfigChange& change) const {
  std::lock_guard<std::mutex> lock(mu_);
  if (scopes_.empty()) return true;
  for (const auto& scope : scopes_) {
    if (scope.matches(change)) return true;
  }
  return false;
}

void ConfigCollector::emit(const ConfigChange& change, ConfigDetectionOrigin origin) {
  std::string dedup_key = std::to_string(static_cast<int>(change.severity)) + ":" +
                          std::to_string(static_cast<int>(change.kind)) + ":" +
                          change.key_path + "\\" + change.value_name;
  if (isDuplicate(dedup_key)) return;
  events_emitted_++;
  if (callback_) {
    callback_(change, origin);
  }
}

}  // namespace monix::collectors::config
