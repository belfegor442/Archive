#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace monix::collectors::sensor {

using SensorId = std::uint64_t;

enum class SensorKind : std::uint8_t {
  CpuUsage,
  GpuUsage,
  GpuTemperature,
  RamUsage,
  Temperature,
  DiskUsage,
  DiskIO,
  NetworkThroughput,
  BatteryLevel,
  PowerDraw
};

const char* SensorKindName(SensorKind k);
std::string SensorKindUnit(SensorKind k);

enum class SensorState : std::uint8_t {
  Normal,
  Warning,
  Critical,
  Unknown
};

const char* SensorStateName(SensorState s);

enum class SensorEventKind : std::uint8_t {
  Warning,
  Critical,
  Clear
};

const char* SensorEventKindName(SensorEventKind k);
std::string SensorEventKindAction(SensorEventKind k);

enum class SensorDetectionOrigin : std::uint8_t {
  WMI,
  PerformanceCounter,
  Polling,
  Manual
};

const char* SensorDetectionOriginName(SensorDetectionOrigin o);

struct SensorThreshold {
  double warning_threshold = 0.0;
  double critical_threshold = 0.0;
  double clear_threshold = 0.0;
  double hysteresis = 0.0;
  double change_threshold = 0.0;

  bool isAboveWarning(double value) const;
  bool isAboveCritical(double value) const;
  bool isBelowClear(double value, SensorState current_state) const;
  bool hasChanged(double old_value, double new_value) const;
};

struct SensorReading {
  SensorId id = 0;
  SensorKind kind = SensorKind::CpuUsage;
  std::string name;
  std::string label;
  double value = 0.0;
  SensorState state = SensorState::Normal;
  SensorThreshold threshold;
  std::int64_t timestamp_ms = 0;
  std::string unit;

  bool isValid() const;
  std::string summary() const;
};

struct SensorEvent {
  SensorId id = 0;
  SensorKind kind = SensorKind::CpuUsage;
  SensorEventKind event_kind = SensorEventKind::Clear;
  std::string name;
  std::string label;
  double value = 0.0;
  SensorState state = SensorState::Normal;
  double previous_value = 0.0;
  SensorState previous_state = SensorState::Normal;
  std::string unit;
  std::int64_t timestamp_ms = 0;

  bool isValid() const;
  bool isWarning() const;
  bool isCritical() const;
  bool isClear() const;
  std::string summary() const;
};

}  // namespace monix::collectors::sensor
