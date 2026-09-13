#include "SensorCollector.hpp"

#include <algorithm>
#include <cmath>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "pdh.lib")

namespace monix::collectors::sensor {

SensorCollector::SensorCollector(SensorCollectorConfig config)
    : config_(std::move(config)) {}

SensorCollector::~SensorCollector() {
  if (running_.load()) {
    stop();
  }
}

bool SensorCollector::start() {
  bool expected = false;
  if (!running_.compare_exchange_strong(expected, true)) {
    return false;
  }
  return true;
}

bool SensorCollector::stop() {
  bool expected = true;
  if (!running_.compare_exchange_strong(expected, false)) {
    return false;
  }
  return true;
}

bool SensorCollector::isRunning() const {
  return running_.load();
}

void SensorCollector::setCallback(Callback cb) {
  std::lock_guard<std::mutex> lock(mu_);
  callback_ = std::move(cb);
}

void SensorCollector::setConfig(const SensorCollectorConfig& config) {
  std::lock_guard<std::mutex> lock(mu_);
  config_ = config;
}

SensorCollectorConfig SensorCollector::config() const {
  std::lock_guard<std::mutex> lock(mu_);
  return config_;
}

void SensorCollector::setThreshold(SensorKind kind, const SensorThreshold& threshold) {
  std::lock_guard<std::mutex> lock(mu_);
  switch (kind) {
    case SensorKind::CpuUsage:         config_.cpu_threshold = threshold; break;
    case SensorKind::GpuUsage:         config_.gpu_threshold = threshold; break;
    case SensorKind::GpuTemperature:   config_.gpu_temp_threshold = threshold; break;
    case SensorKind::RamUsage:         config_.ram_threshold = threshold; break;
    case SensorKind::Temperature:      config_.temp_threshold = threshold; break;
    case SensorKind::DiskUsage:        config_.disk_threshold = threshold; break;
    case SensorKind::BatteryLevel:     config_.battery_threshold = threshold; break;
    case SensorKind::PowerDraw:        config_.power_threshold = threshold; break;
    default: break;
  }
}

std::vector<SensorReading> SensorCollector::snapshot() const {
  std::lock_guard<std::mutex> lock(mu_);
  std::vector<SensorReading> result;
  result.reserve(tracked_.size());
  for (const auto& [key, reading] : tracked_) {
    result.push_back(reading);
  }
  return result;
}

std::size_t SensorCollector::sensorsTracked() const {
  std::lock_guard<std::mutex> lock(mu_);
  return tracked_.size();
}

std::size_t SensorCollector::eventsEmitted() const {
  return events_emitted_.load();
}

const SensorThreshold& getThresholdForKind(const SensorCollectorConfig& config, SensorKind kind) {
  switch (kind) {
    case SensorKind::CpuUsage:         return config.cpu_threshold;
    case SensorKind::GpuUsage:         return config.gpu_threshold;
    case SensorKind::GpuTemperature:   return config.gpu_temp_threshold;
    case SensorKind::RamUsage:         return config.ram_threshold;
    case SensorKind::Temperature:      return config.temp_threshold;
    case SensorKind::DiskUsage:        return config.disk_threshold;
    case SensorKind::BatteryLevel:     return config.battery_threshold;
    case SensorKind::PowerDraw:        return config.power_threshold;
    default:                           return config.cpu_threshold;
  }
}

SensorState SensorCollector::evaluateState(SensorKind kind, double value) const {
  SensorCollectorConfig config;
  {
    std::lock_guard<std::mutex> lock(mu_);
    config = config_;
  }
  const SensorThreshold& thresh = getThresholdForKind(config, kind);
  if (thresh.isAboveCritical(value)) return SensorState::Critical;
  if (thresh.isAboveWarning(value)) return SensorState::Warning;
  return SensorState::Normal;
}

SensorState SensorCollector::processReading(SensorReading& reading) {
  std::string key = std::to_string(static_cast<int>(reading.kind)) + ":" + reading.name;
  if (!reading.label.empty()) key += ":" + reading.label;

  SensorState old_state = SensorState::Normal;
  double old_value = reading.value;
  {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = tracked_.find(key);
    if (it != tracked_.end()) {
      old_state = it->second.state;
      old_value = it->second.value;
    }
  }

  SensorThreshold thresh;
  {
    std::lock_guard<std::mutex> lock(mu_);
    thresh = getThresholdForKind(config_, reading.kind);
  }
  SensorState raw_state = evaluateState(reading.kind, reading.value);
  SensorState new_state = raw_state;

  if (old_state == SensorState::Warning && raw_state == SensorState::Normal) {
    if (!thresh.isBelowClear(reading.value, SensorState::Warning)) {
      new_state = SensorState::Warning;
    }
  } else if (old_state == SensorState::Critical && raw_state != SensorState::Critical) {
    if (!thresh.isBelowClear(reading.value, SensorState::Critical)) {
      new_state = SensorState::Critical;
    }
  }

  bool value_changed = thresh.hasChanged(old_value, reading.value);
  bool state_changed = (new_state != old_state);

  if (!value_changed && !state_changed) {
    return new_state;
  }

  SensorEvent event;
  event.id = reading.id;
  event.kind = reading.kind;
  event.name = reading.name;
  event.label = reading.label;
  event.value = reading.value;
  event.previous_value = old_value;
  event.previous_state = old_state;
  event.unit = SensorKindUnit(reading.kind);
  event.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::system_clock::now().time_since_epoch()).count();

  if (state_changed) {
    if (new_state == SensorState::Warning && old_state == SensorState::Normal) {
      event.event_kind = SensorEventKind::Warning;
      event.state = SensorState::Warning;
      emit(event, SensorDetectionOrigin::Polling);
    } else if (new_state == SensorState::Critical && old_state != SensorState::Critical) {
      event.event_kind = SensorEventKind::Critical;
      event.state = SensorState::Critical;
      emit(event, SensorDetectionOrigin::Polling);
    } else if (new_state == SensorState::Normal && (old_state == SensorState::Warning || old_state == SensorState::Critical)) {
      event.event_kind = SensorEventKind::Clear;
      event.state = SensorState::Normal;
      emit(event, SensorDetectionOrigin::Polling);
    }
  } else if (value_changed && new_state != SensorState::Normal && thresh.change_threshold > 0.0) {
    if (new_state == SensorState::Warning) {
      event.event_kind = SensorEventKind::Warning;
      event.state = SensorState::Warning;
      emit(event, SensorDetectionOrigin::Polling);
    } else if (new_state == SensorState::Critical) {
      event.event_kind = SensorEventKind::Critical;
      event.state = SensorState::Critical;
      emit(event, SensorDetectionOrigin::Polling);
    }
  }

  reading.state = new_state;
  {
    std::lock_guard<std::mutex> lock(mu_);
    tracked_[key] = reading;
  }

  return new_state;
}

bool SensorCollector::pushReading(const SensorReading& reading) {
  if (!running_.load()) return false;
  SensorReading local = reading;
  local.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::system_clock::now().time_since_epoch()).count();
  local.unit = SensorKindUnit(local.kind);
  {
    std::lock_guard<std::mutex> lock(mu_);
    local.threshold = getThresholdForKind(config_, local.kind);
  }
  processReading(local);
  return true;
}

bool SensorCollector::poll() {
  if (!running_.load()) return false;
  return true;
}

bool SensorCollector::isDuplicate(const std::string& key) {
  auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::system_clock::now().time_since_epoch()).count();
  std::lock_guard<std::mutex> lock(mu_);
  const auto window = std::max<std::int64_t>(config_.dedup_window_ms.count(), 1);
  std::string dedup_key = key + "@" + std::to_string(now / window);
  if (dedup_keys_.count(dedup_key) > 0) return true;
  dedup_keys_.insert(dedup_key);
  return false;
}

void SensorCollector::emit(const SensorEvent& event, SensorDetectionOrigin origin) {
  events_emitted_.fetch_add(1, std::memory_order_relaxed);
  Callback callback;
  {
    std::lock_guard<std::mutex> lock(callback_mu_);
    callback = callback_;
  }
  if (callback) callback(event, origin);
}

}  // namespace monix::collectors::sensor
