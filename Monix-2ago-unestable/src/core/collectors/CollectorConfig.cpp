#include "CollectorConfig.hpp"

#include <algorithm>
#include <cmath>

namespace monix::collectors {

const char* CollectorModeName(CollectorMode m) {
  switch (m) {
    case CollectorMode::Passive: return "Passive";
    case CollectorMode::Active:  return "Active";
    case CollectorMode::Hybrid:  return "Hybrid";
  }
  return "Unknown";
}

CollectorMode CollectorModeFromString(const char* s) {
  std::string str(s);
  if (str == "Passive") return CollectorMode::Passive;
  if (str == "Active")  return CollectorMode::Active;
  if (str == "Hybrid")  return CollectorMode::Hybrid;
  return CollectorMode::Passive;
}

const char* CollectorLogLevelName(CollectorLogLevel l) {
  switch (l) {
    case CollectorLogLevel::Silent: return "Silent";
    case CollectorLogLevel::Error:  return "Error";
    case CollectorLogLevel::Warning: return "Warning";
    case CollectorLogLevel::Info:   return "Info";
    case CollectorLogLevel::Debug:  return "Debug";
    case CollectorLogLevel::Trace:  return "Trace";
  }
  return "Unknown";
}

CollectorLogLevel CollectorLogLevelFromString(const char* s) {
  std::string str(s);
  if (str == "Silent") return CollectorLogLevel::Silent;
  if (str == "Error")  return CollectorLogLevel::Error;
  if (str == "Warning") return CollectorLogLevel::Warning;
  if (str == "Info")   return CollectorLogLevel::Info;
  if (str == "Debug")  return CollectorLogLevel::Debug;
  if (str == "Trace")  return CollectorLogLevel::Trace;
  return CollectorLogLevel::Info;
}

const char* ConfigFieldTypeName(ConfigFieldType t) {
  switch (t) {
    case ConfigFieldType::Boolean: return "Boolean";
    case ConfigFieldType::Int64:   return "Int64";
    case ConfigFieldType::UInt64:  return "UInt64";
    case ConfigFieldType::Double:  return "Double";
    case ConfigFieldType::String:  return "String";
  }
  return "Unknown";
}

const char* CollectorConfigFieldName(CollectorConfigField f) {
  switch (f) {
    case CollectorConfigField::Enabled:              return "enabled";
    case CollectorConfigField::SamplingIntervalMs:   return "sampling_interval_ms";
    case CollectorConfigField::MaxEventsPerSecond:   return "max_events_per_second";
    case CollectorConfigField::QueueLimit:           return "queue_limit";
    case CollectorConfigField::AutoRestart:          return "auto_restart";
    case CollectorConfigField::MaxRestartAttempts:   return "max_restart_attempts";
    case CollectorConfigField::RestartBackoffMs:     return "restart_backoff_ms";
    case CollectorConfigField::Mode:                 return "mode";
    case CollectorConfigField::LogLevel:             return "log_level";
  }
  return "unknown";
}

static std::vector<ConfigFieldInfo> buildFieldInfos() {
  std::vector<ConfigFieldInfo> infos;

  infos.push_back({
    CollectorConfigField::Enabled, "enabled", ConfigFieldType::Boolean,
    true, false, false, false
  });

  infos.push_back({
    CollectorConfigField::SamplingIntervalMs, "sampling_interval_ms", ConfigFieldType::Int64,
    std::int64_t(1000), std::int64_t(0), std::int64_t(86400000), true, true
  });

  infos.push_back({
    CollectorConfigField::MaxEventsPerSecond, "max_events_per_second", ConfigFieldType::UInt64,
    std::uint64_t(1000), std::uint64_t(1), std::uint64_t(1000000), true, true
  });

  infos.push_back({
    CollectorConfigField::QueueLimit, "queue_limit", ConfigFieldType::UInt64,
    std::uint64_t(100000), std::uint64_t(100), std::uint64_t(10000000), true, true
  });

  infos.push_back({
    CollectorConfigField::AutoRestart, "auto_restart", ConfigFieldType::Boolean,
    true, false, false, false
  });

  infos.push_back({
    CollectorConfigField::MaxRestartAttempts, "max_restart_attempts", ConfigFieldType::Int64,
    std::int64_t(3), std::int64_t(0), std::int64_t(100), true, true
  });

  infos.push_back({
    CollectorConfigField::RestartBackoffMs, "restart_backoff_ms", ConfigFieldType::Int64,
    std::int64_t(5000), std::int64_t(100), std::int64_t(300000), true, true
  });

  infos.push_back({
    CollectorConfigField::Mode, "mode", ConfigFieldType::Int64,
    std::int64_t(0), std::int64_t(0), std::int64_t(2), true, true
  });

  infos.push_back({
    CollectorConfigField::LogLevel, "log_level", ConfigFieldType::Int64,
    std::int64_t(3), std::int64_t(0), std::int64_t(5), true, true
  });

  return infos;
}

const std::vector<ConfigFieldInfo>& defaultFieldInfos() {
  static const auto infos = buildFieldInfos();
  return infos;
}

CollectorConfig::CollectorConfig() {
  reset();
}

void CollectorConfig::set(CollectorConfigField field, ConfigValue value) {
  std::lock_guard<std::mutex> lock(mu_);
  values_[static_cast<std::uint16_t>(field)] = std::move(value);
}

ConfigValue CollectorConfig::getRaw(CollectorConfigField field) const {
  std::lock_guard<std::mutex> lock(mu_);
  auto it = values_.find(static_cast<std::uint16_t>(field));
  if (it != values_.end()) return it->second;
  for (const auto& info : defaultFieldInfos()) {
    if (info.field == field) return info.default_value;
  }
  return std::string("");
}

ConfigValue CollectorConfig::get(CollectorConfigField field) const {
  return getRaw(field);
}

bool CollectorConfig::has(CollectorConfigField field) const {
  std::lock_guard<std::mutex> lock(mu_);
  return values_.count(static_cast<std::uint16_t>(field)) > 0;
}

void CollectorConfig::remove(CollectorConfigField field) {
  std::lock_guard<std::mutex> lock(mu_);
  values_.erase(static_cast<std::uint16_t>(field));
}

bool CollectorConfig::getBool(CollectorConfigField field, bool def) const {
  auto v = getRaw(field);
  if (std::holds_alternative<bool>(v)) return std::get<bool>(v);
  return def;
}

std::int64_t CollectorConfig::getInt(CollectorConfigField field, std::int64_t def) const {
  auto v = getRaw(field);
  if (std::holds_alternative<std::int64_t>(v)) return std::get<std::int64_t>(v);
  return def;
}

std::uint64_t CollectorConfig::getUInt(CollectorConfigField field, std::uint64_t def) const {
  auto v = getRaw(field);
  if (std::holds_alternative<std::uint64_t>(v)) return std::get<std::uint64_t>(v);
  return def;
}

double CollectorConfig::getDouble(CollectorConfigField field, double def) const {
  auto v = getRaw(field);
  if (std::holds_alternative<double>(v)) return std::get<double>(v);
  return def;
}

std::string CollectorConfig::getString(CollectorConfigField field, const std::string& def) const {
  auto v = getRaw(field);
  if (std::holds_alternative<std::string>(v)) return std::get<std::string>(v);
  return def;
}

void CollectorConfig::setString(CollectorConfigField field, const std::string& value) {
  set(field, value);
}

bool CollectorConfig::enabled() const {
  return getBool(CollectorConfigField::Enabled, true);
}

std::int64_t CollectorConfig::samplingIntervalMs() const {
  return getInt(CollectorConfigField::SamplingIntervalMs, 1000);
}

std::uint64_t CollectorConfig::maxEventsPerSecond() const {
  return getUInt(CollectorConfigField::MaxEventsPerSecond, 1000);
}

std::uint64_t CollectorConfig::queueLimit() const {
  return getUInt(CollectorConfigField::QueueLimit, 100000);
}

bool CollectorConfig::autoRestart() const {
  return getBool(CollectorConfigField::AutoRestart, true);
}

std::uint32_t CollectorConfig::maxRestartAttempts() const {
  return static_cast<std::uint32_t>(
    getInt(CollectorConfigField::MaxRestartAttempts, 3));
}

std::uint64_t CollectorConfig::restartBackoffMs() const {
  return static_cast<std::uint64_t>(
    getInt(CollectorConfigField::RestartBackoffMs, 5000));
}

CollectorMode CollectorConfig::mode() const {
  auto v = getRaw(CollectorConfigField::Mode);
  if (std::holds_alternative<std::int64_t>(v)) {
    auto val = std::get<std::int64_t>(v);
    if (val >= 0 && val <= 2) return static_cast<CollectorMode>(val);
  }
  return CollectorMode::Passive;
}

CollectorLogLevel CollectorConfig::logLevel() const {
  auto v = getRaw(CollectorConfigField::LogLevel);
  if (std::holds_alternative<std::int64_t>(v)) {
    auto val = std::get<std::int64_t>(v);
    if (val >= 0 && val <= 5) return static_cast<CollectorLogLevel>(val);
  }
  return CollectorLogLevel::Info;
}

std::vector<ConfigValidationIssue> CollectorConfig::validate() const {
  std::vector<ConfigValidationIssue> issues;

  for (const auto& info : defaultFieldInfos()) {
    auto v = getRaw(info.field);
    bool typeOk = false;
    switch (info.type) {
      case ConfigFieldType::Boolean:
        typeOk = std::holds_alternative<bool>(v);
        break;
      case ConfigFieldType::Int64:
        typeOk = std::holds_alternative<std::int64_t>(v);
        if (typeOk && info.has_min) {
          if (std::get<std::int64_t>(v) < std::get<std::int64_t>(info.min_value)) {
            issues.push_back({info.field, "Value below minimum"});
            typeOk = false;
          }
        }
        if (typeOk && info.has_max) {
          if (std::get<std::int64_t>(v) > std::get<std::int64_t>(info.max_value)) {
            issues.push_back({info.field, "Value above maximum"});
            typeOk = false;
          }
        }
        break;
      case ConfigFieldType::UInt64:
        typeOk = std::holds_alternative<std::uint64_t>(v);
        if (typeOk && info.has_min) {
          if (std::get<std::uint64_t>(v) < std::get<std::uint64_t>(info.min_value)) {
            issues.push_back({info.field, "Value below minimum"});
            typeOk = false;
          }
        }
        if (typeOk && info.has_max) {
          if (std::get<std::uint64_t>(v) > std::get<std::uint64_t>(info.max_value)) {
            issues.push_back({info.field, "Value above maximum"});
            typeOk = false;
          }
        }
        break;
      case ConfigFieldType::Double:
        typeOk = std::holds_alternative<double>(v);
        break;
      case ConfigFieldType::String:
        typeOk = std::holds_alternative<std::string>(v);
        break;
    }
    if (!typeOk && issues.empty()) {
      issues.push_back({info.field, "Invalid type for field"});
    }
  }

  return issues;
}

bool CollectorConfig::isValid() const {
  return validate().empty();
}

void CollectorConfig::merge(const CollectorConfig& other) {
  std::lock_guard<std::mutex> lock(mu_);
  std::lock_guard<std::mutex> otherLock(other.mu_);
  for (const auto& [key, value] : other.values_) {
    values_[key] = value;
  }
}

void CollectorConfig::reset() {
  std::lock_guard<std::mutex> lock(mu_);
  values_.clear();
  for (const auto& info : defaultFieldInfos()) {
    values_[static_cast<std::uint16_t>(info.field)] = info.default_value;
  }
}

std::size_t CollectorConfig::fieldCount() const {
  std::lock_guard<std::mutex> lock(mu_);
  return values_.size();
}

}  // namespace monix::collectors
