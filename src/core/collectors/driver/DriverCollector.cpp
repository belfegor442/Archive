#include "DriverCollector.hpp"

#include <algorithm>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <psapi.h>

#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "psapi.lib")

namespace monix::collectors::driver {

DriverCollector::DriverCollector(DriverCollectorConfig config)
    : config_(std::move(config)) {}

DriverCollector::~DriverCollector() {
  if (running_.load()) {
    stop();
  }
}

bool DriverCollector::start() {
  bool expected = false;
  if (!running_.compare_exchange_strong(expected, true)) {
    return false;
  }
  takeSnapshot();
  return true;
}

bool DriverCollector::stop() {
  bool expected = true;
  if (!running_.compare_exchange_strong(expected, false)) {
    return false;
  }
  return true;
}

bool DriverCollector::isRunning() const {
  return running_.load();
}

void DriverCollector::setCallback(Callback cb) {
  std::lock_guard<std::mutex> lock(mu_);
  callback_ = std::move(cb);
}

void DriverCollector::setConfig(const DriverCollectorConfig& config) {
  std::lock_guard<std::mutex> lock(mu_);
  config_ = config;
}

const DriverCollectorConfig& DriverCollector::config() const {
  return config_;
}

std::vector<DriverInfo> DriverCollector::snapshot() const {
  std::lock_guard<std::mutex> lock(mu_);
  std::vector<DriverInfo> result;
  result.reserve(tracked_.size());
  for (const auto& [key, info] : tracked_) {
    result.push_back(info);
  }
  return result;
}

std::size_t DriverCollector::driversTracked() const {
  std::lock_guard<std::mutex> lock(mu_);
  return tracked_.size();
}

std::size_t DriverCollector::eventsEmitted() const {
  return events_emitted_.load();
}

DriverInfo DriverCollector::buildDriverInfo(const std::string& name, const std::string& path,
    std::uint64_t base_address, DriverState state) const {
  DriverInfo info;
  info.name = name;
  info.path = path;
  info.base_address = base_address;
  info.state = state;
  info.id = std::hash<std::string>{}(name);
  info.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::system_clock::now().time_since_epoch()).count();
  return info;
}

std::vector<DriverInfo> DriverCollector::enumerateDriversFromRegistry() const {
  std::vector<DriverInfo> result;

  HKEY services_key = nullptr;
  if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Services", 0, KEY_READ, &services_key) != ERROR_SUCCESS) {
    return result;
  }

  DWORD subkey_count = 0;
  RegQueryInfoKeyW(services_key, nullptr, nullptr, nullptr, &subkey_count,
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);

  for (DWORD i = 0; i < subkey_count; i++) {
    wchar_t subkey_name[256]{};
    DWORD name_len = 256;
    if (RegEnumKeyExW(services_key, i, subkey_name, &name_len, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS) {
      continue;
    }

    HKEY svc_key = nullptr;
    if (RegOpenKeyExW(services_key, subkey_name, 0, KEY_READ, &svc_key) != ERROR_SUCCESS) {
      continue;
    }

    DWORD svc_type = 0;
    DWORD type_size = sizeof(svc_type);
    if (RegQueryValueExW(svc_key, L"Type", nullptr, nullptr, reinterpret_cast<LPBYTE>(&svc_type), &type_size) != ERROR_SUCCESS) {
      RegCloseKey(svc_key);
      continue;
    }

    if ((svc_type & 0x1) == 0) {
      RegCloseKey(svc_key);
      continue;
    }

    DWORD start_val = 0;
    DWORD start_size = sizeof(start_val);
    RegQueryValueExW(svc_key, L"Start", nullptr, nullptr, reinterpret_cast<LPBYTE>(&start_val), &start_size);

    DriverState state = DriverState::Unknown;
    if (start_val == 0) state = DriverState::Running;
    else if (start_val == 3) state = DriverState::Running;
    else if (start_val == 4) state = DriverState::Stopped;

    std::string svc_name;
    {
      int sz = WideCharToMultiByte(CP_UTF8, 0, subkey_name, -1, nullptr, 0, nullptr, nullptr);
      if (sz > 0) {
        svc_name.resize(static_cast<std::size_t>(sz) - 1);
        WideCharToMultiByte(CP_UTF8, 0, subkey_name, -1, svc_name.data(), sz, nullptr, nullptr);
      }
    }

    std::string image_path;
    wchar_t image_path_buf[MAX_PATH]{};
    DWORD path_size = sizeof(image_path_buf);
    if (RegQueryValueExW(svc_key, L"ImagePath", nullptr, nullptr, reinterpret_cast<LPBYTE>(image_path_buf), &path_size) == ERROR_SUCCESS) {
      int sz = WideCharToMultiByte(CP_UTF8, 0, image_path_buf, -1, nullptr, 0, nullptr, nullptr);
      if (sz > 0) {
        image_path.resize(static_cast<std::size_t>(sz) - 1);
        WideCharToMultiByte(CP_UTF8, 0, image_path_buf, -1, image_path.data(), sz, nullptr, nullptr);
      }
    }

    std::string provider_name;
    wchar_t provider_buf[256]{};
    DWORD provider_size = sizeof(provider_buf);
    if (RegQueryValueExW(svc_key, L"ProviderName", nullptr, nullptr, reinterpret_cast<LPBYTE>(provider_buf), &provider_size) == ERROR_SUCCESS) {
      int sz = WideCharToMultiByte(CP_UTF8, 0, provider_buf, -1, nullptr, 0, nullptr, nullptr);
      if (sz > 0) {
        provider_name.resize(static_cast<std::size_t>(sz) - 1);
        WideCharToMultiByte(CP_UTF8, 0, provider_buf, -1, provider_name.data(), sz, nullptr, nullptr);
      }
    }

    std::string version_str;
    HKEY version_key = nullptr;
    if (RegOpenKeyExW(svc_key, L"Enumerations", 0, KEY_READ, &version_key) == ERROR_SUCCESS) {
      RegCloseKey(version_key);
    }

    RegCloseKey(svc_key);

    DriverInfo info = buildDriverInfo(svc_name, image_path, 0, state);
    info.provider = provider_name;
    info.version = version_str;
    result.push_back(std::move(info));
  }

  RegCloseKey(services_key);
  return result;
}

std::vector<DriverInfo> DriverCollector::enumerateDriversFromAPI() const {
  std::vector<DriverInfo> result;

  DWORD bytes_needed = 0;
  EnumDeviceDrivers(nullptr, 0, &bytes_needed);
  if (bytes_needed == 0) return result;

  DWORD driver_count = bytes_needed / sizeof(LPVOID);
  std::vector<LPVOID> base_addresses(driver_count);
  DWORD bytes_returned = 0;
  if (!EnumDeviceDrivers(base_addresses.data(), bytes_needed, &bytes_returned)) {
    return result;
  }

  driver_count = bytes_returned / sizeof(LPVOID);

  for (DWORD i = 0; i < driver_count; i++) {
    char base_name[256]{};
    GetDeviceDriverBaseNameA(base_addresses[i], base_name, sizeof(base_name));

    char driver_path[512]{};
    GetDeviceDriverFileNameA(base_addresses[i], driver_path, sizeof(driver_path));

    std::string name(base_name);
    std::string path(driver_path);

    if (!name.empty()) {
      result.push_back(buildDriverInfo(name, path, reinterpret_cast<std::uint64_t>(base_addresses[i]), DriverState::Running));
    }
  }

  return result;
}

bool DriverCollector::takeSnapshot() {
  auto registry_drivers = enumerateDriversFromRegistry();
  auto api_drivers = enumerateDriversFromAPI();

  std::unordered_map<std::string, std::uint64_t> api_bases;
  for (const auto& d : api_drivers) {
    api_bases[d.name] = d.base_address;
  }

  for (auto& d : registry_drivers) {
    auto it = api_bases.find(d.name);
    if (it != api_bases.end()) {
      d.base_address = it->second;
      if (d.state == DriverState::Unknown) {
        d.state = DriverState::Running;
      }
    } else if (d.state == DriverState::Unknown) {
      d.state = DriverState::Stopped;
    }
  }

  std::unordered_set<std::string> current_names;
  std::vector<std::pair<DriverEventKind, DriverInfo>> pending;

  for (const auto& info : registry_drivers) {
    std::string key = info.name;
    current_names.insert(key);

    std::lock_guard<std::mutex> lock(mu_);
    auto it = tracked_.find(key);
    if (it == tracked_.end()) {
      tracked_[key] = info;
      pending.push_back({DriverEventKind::Loaded, info});
    } else {
      bool changed = false;
      if (it->second.state != info.state) changed = true;
      if (config_.include_path && it->second.path != info.path) changed = true;
      if (config_.include_version && it->second.version != info.version) changed = true;
      if (config_.include_provider && it->second.provider != info.provider) changed = true;

      it->second = info;
      if (changed) {
        pending.push_back({DriverEventKind::Changed, info});
      }
    }
  }

  {
    std::lock_guard<std::mutex> lock(mu_);
    std::vector<std::string> to_remove;
    for (const auto& [key, info] : tracked_) {
      if (current_names.find(key) == current_names.end()) {
        to_remove.push_back(key);
        pending.push_back({DriverEventKind::Unloaded, info});
      }
    }
    for (const auto& key : to_remove) {
      tracked_.erase(key);
    }
  }

  for (const auto& [kind, info] : pending) {
    emit(kind, DriverDetectionOrigin::Registry, info);
  }

  return true;
}

bool DriverCollector::poll() {
  if (!running_.load()) return false;
  return takeSnapshot();
}

std::vector<DriverEventKind> DriverCollector::pendingEvents() {
  return {};
}

bool DriverCollector::isDuplicate(DriverEventKind kind, const std::string& key) {
  auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::system_clock::now().time_since_epoch()).count();

  std::string dedup_key = std::to_string(static_cast<int>(kind)) + ":" + key +
                          "@" + std::to_string(now / config_.dedup_window_ms.count());

  std::lock_guard<std::mutex> lock(mu_);
  if (dedup_keys_.count(dedup_key) > 0) {
    return true;
  }
  dedup_keys_.insert(dedup_key);
  return false;
}

void DriverCollector::applyConfigFilter(DriverEventKind& kind) {
  if (!config_.track_loaded && kind == DriverEventKind::Loaded) kind = DriverEventKind::Changed;
  if (!config_.track_unloaded && kind == DriverEventKind::Unloaded) kind = DriverEventKind::Changed;
  if (!config_.track_changed && kind == DriverEventKind::Changed) kind = DriverEventKind::Loaded;
}

void DriverCollector::emit(DriverEventKind kind, DriverDetectionOrigin origin, const DriverInfo& info) {
  applyConfigFilter(kind);
  if (isDuplicate(kind, info.name)) return;
  events_emitted_++;
  if (callback_) {
    callback_(kind, origin, info);
  }
}

}  // namespace monix::collectors::driver
