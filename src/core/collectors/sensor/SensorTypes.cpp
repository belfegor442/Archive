#include "SensorTypes.hpp"

#include <cmath>

namespace monix::collectors::sensor {

const char* SensorKindName(SensorKind k) {
  switch (k) {
    case SensorKind::CpuUsage:         return "CpuUsage";
    case SensorKind::GpuUsage:         return "GpuUsage";
    case SensorKind::GpuTemperature:   return "GpuTemperature";
    case SensorKind::RamUsage:         return "RamUsage";
    case SensorKind::Temperature:      return "Temperature";
    case SensorKind::DiskUsage:        return "DiskUsage";
    case SensorKind::DiskIO:           return "DiskIO";
    case SensorKind::NetworkThroughput: return "NetworkThroughput";
    case SensorKind::BatteryLevel:     return "BatteryLevel";
    case SensorKind::PowerDraw:        return "PowerDraw";
  }
  return "Unknown";
}

std::string SensorKindUnit(SensorKind k) {
  switch (k) {
    case SensorKind::CpuUsage:
    case SensorKind::GpuUsage:
    case SensorKind::RamUsage:
    case SensorKind::DiskUsage:
    case SensorKind::BatteryLevel:     return "%";
    case SensorKind::GpuTemperature:
    case SensorKind::Temperature:      return "°C";
    case SensorKind::DiskIO:
    case SensorKind::NetworkThroughput: return "MB/s";
    case SensorKind::PowerDraw:        return "W";
  }
  return "";
}

const char* SensorStateName(SensorState s) {
  switch (s) {
    case SensorState::Normal:   return "Normal";
    case SensorState::Warning:  return "Warning";
    case SensorState::Critical: return "Critical";
    case SensorState::Unknown:  return "Unknown";
  }
  return "Unknown";
}

const char* SensorEventKindName(SensorEventKind k) {
  switch (k) {
    case SensorEventKind::Warning:  return "Warning";
    case SensorEventKind::Critical: return "Critical";
    case SensorEventKind::Clear:    return "Clear";
  }
  return "Unknown";
}

std::string SensorEventKindAction(SensorEventKind k) {
  switch (k) {
    case SensorEventKind::Warning:  return "sensor.warning";
    case SensorEventKind::Critical: return "sensor.critical";
    case SensorEventKind::Clear:    return "sensor.clear";
  }
  return "unknown";
}

const char* SensorDetectionOriginName(SensorDetectionOrigin o) {
  switch (o) {
    case SensorDetectionOrigin::WMI:               return "WMI";
    case SensorDetectionOrigin::PerformanceCounter: return "PerformanceCounter";
    case SensorDetectionOrigin::Polling:            return "Polling";
    case SensorDetectionOrigin::Manual:             return "Manual";
  }
  return "Unknown";
}

bool SensorThreshold::isAboveWarning(double value) const {
  if (warning_threshold <= 0.0) return false;
  return value >= warning_threshold;
}

bool SensorThreshold::isAboveCritical(double value) const {
  if (critical_threshold <= 0.0) return false;
  return value >= critical_threshold;
}

bool SensorThreshold::isBelowClear(double value, SensorState current_state) const {
  if (current_state == SensorState::Critical) {
    return value <= (critical_threshold - hysteresis);
  }
  if (current_state == SensorState::Warning) {
    return value <= (warning_threshold - hysteresis);
  }
  return false;
}

bool SensorThreshold::hasChanged(double old_value, double new_value) const {
  if (change_threshold <= 0.0) return true;
  return std::fabs(new_value - old_value) >= change_threshold;
}

bool SensorReading::isValid() const {
  return !name.empty();
}

std::string SensorReading::summary() const {
  std::string result = name;
  if (!label.empty()) result += " (" + label + ")";
  result += ": " + std::to_string(static_cast<int>(value)) + unit;
  result += " [" + std::string(SensorStateName(state)) + "]";
  return result;
}

bool SensorEvent::isValid() const {
  return !name.empty();
}

bool SensorEvent::isWarning() const {
  return event_kind == SensorEventKind::Warning;
}

bool SensorEvent::isCritical() const {
  return event_kind == SensorEventKind::Critical;
}

bool SensorEvent::isClear() const {
  return event_kind == SensorEventKind::Clear;
}

std::string SensorEvent::summary() const {
  std::string result = std::string(SensorEventKindName(event_kind)) + " ";
  result += name;
  if (!label.empty()) result += " (" + label + ")";
  result += ": " + std::to_string(static_cast<int>(value)) + unit;
  return result;
}

}  // namespace monix::collectors::sensor
