#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "DriverTypes.hpp"

namespace monix::collectors::driver {

struct DriverCollectorConfig {
  bool track_loaded = true;
  bool track_unloaded = true;
  bool track_changed = true;
  bool include_version = true;
  bool include_provider = true;
  bool include_path = true;
  std::chrono::milliseconds poll_interval_ms{5000};
  std::chrono::milliseconds dedup_window_ms{2000};
};

class DriverCollector {
public:
  using Callback = std::function<void(DriverEventKind, DriverDetectionOrigin, const DriverInfo&)>;

  explicit DriverCollector(DriverCollectorConfig config = {});
  ~DriverCollector();

  DriverCollector(const DriverCollector&) = delete;
  DriverCollector& operator=(const DriverCollector&) = delete;

  bool start();
  bool stop();
  bool isRunning() const;

  void setCallback(Callback cb);
  void setConfig(const DriverCollectorConfig& config);
  const DriverCollectorConfig& config() const;

  std::vector<DriverInfo> snapshot() const;
  std::size_t driversTracked() const;
  std::size_t eventsEmitted() const;

  bool takeSnapshot();
  bool poll();
  std::vector<DriverEventKind> pendingEvents();

private:
  DriverInfo buildDriverInfo(const std::string& name, const std::string& path,
    std::uint64_t base_address, DriverState state) const;
  std::vector<DriverInfo> enumerateDriversFromRegistry() const;
  std::vector<DriverInfo> enumerateDriversFromAPI() const;

  bool isDuplicate(DriverEventKind kind, const std::string& key);
  void emit(DriverEventKind kind, DriverDetectionOrigin origin, const DriverInfo& info);
  void applyConfigFilter(DriverEventKind& kind);

  DriverCollectorConfig config_;
  Callback callback_;
  mutable std::mutex mu_;
  std::atomic<bool> running_{false};
  std::unordered_map<std::string, DriverInfo> tracked_;
  std::unordered_set<std::string> dedup_keys_;
  std::atomic<std::size_t> events_emitted_{0};
};

}  // namespace monix::collectors::driver
