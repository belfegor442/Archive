#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "SensorTypes.hpp"

namespace monix::collectors::sensor {

struct SensorCollectorConfig {
  std::chrono::milliseconds sampling_interval_ms{5000};
  std::chrono::milliseconds dedup_window_ms{2000};
  SensorThreshold cpu_threshold;
  SensorThreshold gpu_threshold;
  SensorThreshold gpu_temp_threshold;
  SensorThreshold ram_threshold;
  SensorThreshold temp_threshold;
  SensorThreshold disk_threshold;
  SensorThreshold battery_threshold;
  SensorThreshold power_threshold;
};

class SensorCollector {
public:
  using Callback = std::function<void(const SensorEvent&, SensorDetectionOrigin)>;

  explicit SensorCollector(SensorCollectorConfig config = {});
  ~SensorCollector();

  SensorCollector(const SensorCollector&) = delete;
  SensorCollector& operator=(const SensorCollector&) = delete;

  bool start();
  bool stop();
  bool isRunning() const;

  void setCallback(Callback cb);
  void setConfig(const SensorCollectorConfig& config);
  SensorCollectorConfig config() const;

  void setThreshold(SensorKind kind, const SensorThreshold& threshold);

  std::vector<SensorReading> snapshot() const;
  std::size_t sensorsTracked() const;
  std::size_t eventsEmitted() const;

  bool poll();
  bool pushReading(const SensorReading& reading);

private:
  SensorState evaluateState(SensorKind kind, double value) const;
  SensorState processReading(SensorReading& reading);
  bool isDuplicate(const std::string& key);
  void emit(const SensorEvent& event, SensorDetectionOrigin origin);

  SensorCollectorConfig config_;
  Callback callback_;
  mutable std::mutex mu_;
  mutable std::mutex callback_mu_;
  std::atomic<bool> running_{false};
  std::unordered_map<std::string, SensorReading> tracked_;
  std::unordered_set<std::string> dedup_keys_;
  std::atomic<std::size_t> events_emitted_{0};
};

}  // namespace monix::collectors::sensor
