#include "ServiceCollector.hpp"

#include <algorithm>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winsvc.h>
#include <lm.h>
#include <sddl.h>

#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "netapi32.lib")

namespace monix::collectors::service {

ServiceCollector::ServiceCollector(ServiceCollectorConfig config)
    : config_(std::move(config)) {}

ServiceCollector::~ServiceCollector() {
  if (running_.load()) {
    stop();
  }
}

bool ServiceCollector::start() {
  bool expected = false;
  if (!running_.compare_exchange_strong(expected, true)) {
    return false;
  }
  takeSnapshot();
  return true;
}

bool ServiceCollector::stop() {
  bool expected = true;
  if (!running_.compare_exchange_strong(expected, false)) {
    return false;
  }
  return true;
}

bool ServiceCollector::isRunning() const {
  return running_.load();
}

void ServiceCollector::setCallback(Callback cb) {
  std::lock_guard<std::mutex> lock(mu_);
  callback_ = std::move(cb);
}

void ServiceCollector::setConfig(const ServiceCollectorConfig& config) {
  std::lock_guard<std::mutex> lock(mu_);
  config_ = config;
}

const ServiceCollectorConfig& ServiceCollector::config() const {
  return config_;
}

std::vector<ServiceInfo> ServiceCollector::snapshot() const {
  std::lock_guard<std::mutex> lock(mu_);
  std::vector<ServiceInfo> result;
  result.reserve(tracked_.size());
  for (const auto& [key, info] : tracked_) {
    result.push_back(info);
  }
  return result;
}

std::size_t ServiceCollector::servicesTracked() const {
  std::lock_guard<std::mutex> lock(mu_);
  return tracked_.size();
}

std::size_t ServiceCollector::eventsEmitted() const {
  return events_emitted_.load();
}

ServiceInfo ServiceCollector::createServiceInfo(
    const std::wstring& name, const std::wstring& display_name,
    DWORD state, DWORD start_type, DWORD pid,
    const std::wstring& account, const std::wstring& binary_path) {

  ServiceInfo info;
  info.state = ServiceStateFromDword(state);
  info.start_type = ServiceStartTypeFromDword(start_type);
  info.process_id = pid;
  info.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::system_clock::now().time_since_epoch()).count();

  if (!name.empty()) {
    int sz = WideCharToMultiByte(CP_UTF8, 0, name.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (sz > 0) {
      info.name.resize(static_cast<std::size_t>(sz) - 1);
      WideCharToMultiByte(CP_UTF8, 0, name.c_str(), -1, info.name.data(), sz, nullptr, nullptr);
    }
  }

  if (!display_name.empty()) {
    int sz = WideCharToMultiByte(CP_UTF8, 0, display_name.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (sz > 0) {
      info.display_name.resize(static_cast<std::size_t>(sz) - 1);
      WideCharToMultiByte(CP_UTF8, 0, display_name.c_str(), -1, info.display_name.data(), sz, nullptr, nullptr);
    }
  }

  if (config_.include_account && !account.empty()) {
    int sz = WideCharToMultiByte(CP_UTF8, 0, account.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (sz > 0) {
      info.account.resize(static_cast<std::size_t>(sz) - 1);
      WideCharToMultiByte(CP_UTF8, 0, account.c_str(), -1, info.account.data(), sz, nullptr, nullptr);
    }
  }

  if (config_.include_binary_path && !binary_path.empty()) {
    int sz = WideCharToMultiByte(CP_UTF8, 0, binary_path.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (sz > 0) {
      info.binary_path.resize(static_cast<std::size_t>(sz) - 1);
      WideCharToMultiByte(CP_UTF8, 0, binary_path.c_str(), -1, info.binary_path.data(), sz, nullptr, nullptr);
    }
  }

  info.id = std::hash<std::string>{}(info.name);
  return info;
}

bool ServiceCollector::takeSnapshot() {
  SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_ENUMERATE_SERVICE);
  if (!scm) return false;

  DWORD bytes_needed = 0;
  DWORD services_returned = 0;
  DWORD resume_handle = 0;

  EnumServicesStatusExW(
    scm, SC_ENUM_PROCESS_INFO, SERVICE_WIN32, SERVICE_STATE_ALL,
    nullptr, 0, &bytes_needed, &services_returned, &resume_handle, nullptr);

  if (bytes_needed == 0) {
    CloseServiceHandle(scm);
    return false;
  }

  std::vector<BYTE> buf(bytes_needed);
  if (!EnumServicesStatusExW(
        scm, SC_ENUM_PROCESS_INFO, SERVICE_WIN32, SERVICE_STATE_ALL,
        buf.data(), bytes_needed, &bytes_needed, &services_returned, &resume_handle, nullptr)) {
    CloseServiceHandle(scm);
    return false;
  }

  auto* enum_status = reinterpret_cast<LPENUM_SERVICE_STATUS_PROCESSW>(buf.data());

  struct PendingEvent {
    ServiceEventKind kind;
    ServiceInfo info;
  };
  std::vector<PendingEvent> pending;

  std::unordered_set<std::string> current_names;
  for (DWORD i = 0; i < services_returned; i++) {
    auto& svc = enum_status[i];

    std::wstring wname(svc.lpServiceName);
    std::wstring wdisplay(svc.lpDisplayName);
    DWORD state = svc.ServiceStatusProcess.dwCurrentState;
    DWORD pid = svc.ServiceStatusProcess.dwProcessId;

    std::wstring waccount;
    std::wstring wbinary;
    DWORD start_type = SERVICE_DEMAND_START;
    SC_HANDLE svc_handle = OpenServiceW(scm, svc.lpServiceName, SERVICE_QUERY_CONFIG);
    if (svc_handle) {
      DWORD cfg_bytes = 0;
      QueryServiceConfigW(svc_handle, nullptr, 0, &cfg_bytes);
      if (cfg_bytes > 0) {
        std::vector<BYTE> cfg_buf(cfg_bytes);
        if (QueryServiceConfigW(svc_handle, reinterpret_cast<LPQUERY_SERVICE_CONFIGW>(cfg_buf.data()), cfg_bytes, &cfg_bytes)) {
          auto* cfg = reinterpret_cast<LPQUERY_SERVICE_CONFIGW>(cfg_buf.data());
          if (cfg->lpServiceStartName) waccount = cfg->lpServiceStartName;
          if (cfg->lpBinaryPathName) wbinary = cfg->lpBinaryPathName;
          start_type = cfg->dwStartType;
        }
      }
      CloseServiceHandle(svc_handle);
    }

    ServiceInfo info = createServiceInfo(wname, wdisplay, state,
      start_type, pid, waccount, wbinary);

    std::string key = info.name;
    current_names.insert(key);

    {
      std::lock_guard<std::mutex> lock(mu_);
      auto it = tracked_.find(key);
      if (it == tracked_.end()) {
        tracked_[key] = info;
        pending.push_back({ServiceEventKind::Created, info});
      } else {
        ServiceState old_state = it->second.state;
        it->second = info;
        if (old_state == ServiceState::Stopped && info.state == ServiceState::Running) {
          pending.push_back({ServiceEventKind::Started, info});
        } else if (old_state == ServiceState::Running && info.state == ServiceState::Stopped) {
          pending.push_back({ServiceEventKind::Stopped, info});
        } else if (old_state != info.state) {
          pending.push_back({ServiceEventKind::Changed, info});
        }
      }
    }
  }

  {
    std::lock_guard<std::mutex> lock(mu_);
    std::vector<std::string> to_remove;
    for (const auto& [key, info] : tracked_) {
      if (current_names.find(key) == current_names.end()) {
        to_remove.push_back(key);
        pending.push_back({ServiceEventKind::Deleted, info});
      }
    }
    for (const auto& key : to_remove) {
      tracked_.erase(key);
    }
  }

  for (const auto& evt : pending) {
    emit(evt.kind, ServiceDetectionOrigin::SCM, evt.info);
  }

  CloseServiceHandle(scm);
  return true;
}

bool ServiceCollector::poll() {
  if (!running_.load()) return false;
  return takeSnapshot();
}

std::vector<ServiceEventKind> ServiceCollector::pendingEvents() {
  return {};
}

bool ServiceCollector::isDuplicate(ServiceEventKind kind, const std::string& key) {
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

void ServiceCollector::applyConfigFilter(ServiceEventKind& kind) {
  if (!config_.track_created && kind == ServiceEventKind::Created) kind = ServiceEventKind::Changed;
  if (!config_.track_deleted && kind == ServiceEventKind::Deleted) kind = ServiceEventKind::Changed;
  if (!config_.track_started && kind == ServiceEventKind::Started) kind = ServiceEventKind::Changed;
  if (!config_.track_stopped && kind == ServiceEventKind::Stopped) kind = ServiceEventKind::Changed;
  if (!config_.track_changed && kind == ServiceEventKind::Changed) kind = ServiceEventKind::Started;
}

void ServiceCollector::emit(ServiceEventKind kind, ServiceDetectionOrigin origin, const ServiceInfo& info) {
  applyConfigFilter(kind);
  if (isDuplicate(kind, info.name)) return;
  events_emitted_++;
  if (callback_) {
    callback_(kind, origin, info);
  }
}

}  // namespace monix::collectors::service
