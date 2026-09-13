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
#include <windows.h>

#include "ServiceTypes.hpp"

namespace monix::collectors::service {

struct ServiceCollectorConfig {
  bool track_created = true;
  bool track_deleted = true;
  bool track_started = true;
  bool track_stopped = true;
  bool track_changed = true;
  bool include_account = false;
  bool include_binary_path = false;
  bool include_description = false;
  std::chrono::milliseconds poll_interval_ms{5000};
  std::chrono::milliseconds dedup_window_ms{2000};
};

class ServiceCollector {
public:
  using Callback = std::function<void(ServiceEventKind, ServiceDetectionOrigin, const ServiceInfo&)>;

  explicit ServiceCollector(ServiceCollectorConfig config = {});
  ~ServiceCollector();

  ServiceCollector(const ServiceCollector&) = delete;
  ServiceCollector& operator=(const ServiceCollector&) = delete;

  bool start();
  bool stop();
  bool isRunning() const;

  void setCallback(Callback cb);
  void setConfig(const ServiceCollectorConfig& config);
  const ServiceCollectorConfig& config() const;

  std::vector<ServiceInfo> snapshot() const;
  std::size_t servicesTracked() const;
  std::size_t eventsEmitted() const;

  bool takeSnapshot();
  bool poll();
  std::vector<ServiceEventKind> pendingEvents();

private:
  ServiceInfo createServiceInfo(const std::wstring& name, const std::wstring& display_name,
    DWORD state, DWORD start_type, DWORD pid, const std::wstring& account,
    const std::wstring& binary_path);

  bool isDuplicate(ServiceEventKind kind, const std::string& key);
  void emit(ServiceEventKind kind, ServiceDetectionOrigin origin, const ServiceInfo& info);
  void applyConfigFilter(ServiceEventKind& kind);

  ServiceCollectorConfig config_;
  Callback callback_;
  mutable std::mutex mu_;
  std::atomic<bool> running_{false};
  std::unordered_map<std::string, ServiceInfo> tracked_;
  std::unordered_set<std::string> dedup_keys_;
  std::atomic<std::size_t> events_emitted_{0};
};

}  // namespace monix::collectors::service
