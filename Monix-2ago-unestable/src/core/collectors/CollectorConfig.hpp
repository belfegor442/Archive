#pragma once

#include "CollectorCapabilities.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <variant>
#include <mutex>
#include <vector>

namespace monix::collectors {

enum class CollectorMode : std::uint8_t {
  Passive,
  Active,
  Hybrid
};

const char* CollectorModeName(CollectorMode m);
CollectorMode CollectorModeFromString(const char* s);

enum class CollectorLogLevel : std::uint8_t {
  Silent,
  Error,
  Warning,
  Info,
  Debug,
  Trace
};

const char* CollectorLogLevelName(CollectorLogLevel l);
CollectorLogLevel CollectorLogLevelFromString(const char* s);

enum class ConfigFieldType : std::uint8_t {
  Boolean,
  Int64,
  UInt64,
  Double,
  String
};

const char* ConfigFieldTypeName(ConfigFieldType t);

enum class CollectorConfigField : std::uint16_t {
  Enabled = 1,
  SamplingIntervalMs,
  MaxEventsPerSecond,
  QueueLimit,
  AutoRestart,
  MaxRestartAttempts,
  RestartBackoffMs,
  Mode,
  LogLevel
};

const char* CollectorConfigFieldName(CollectorConfigField f);

using ConfigValue = std::variant<std::string, std::int64_t, std::uint64_t, double, bool>;

struct ConfigFieldInfo {
  CollectorConfigField field;
  const char* name;
  ConfigFieldType type;
  ConfigValue default_value;
  ConfigValue min_value;
  ConfigValue max_value;
  bool has_min = false;
  bool has_max = false;
};

const std::vector<ConfigFieldInfo>& defaultFieldInfos();

struct ConfigValidationIssue {
  CollectorConfigField field = CollectorConfigField::Enabled;
  std::string message;
};

class CollectorConfig {
public:
  CollectorConfig();

  void set(CollectorConfigField field, ConfigValue value);
  ConfigValue get(CollectorConfigField field) const;
  bool has(CollectorConfigField field) const;
  void remove(CollectorConfigField field);

  bool getBool(CollectorConfigField field, bool def = false) const;
  std::int64_t getInt(CollectorConfigField field, std::int64_t def = 0) const;
  std::uint64_t getUInt(CollectorConfigField field, std::uint64_t def = 0) const;
  double getDouble(CollectorConfigField field, double def = 0.0) const;
  std::string getString(CollectorConfigField field, const std::string& def = "") const;

  void setString(CollectorConfigField field, const std::string& value);

  bool enabled() const;
  std::int64_t samplingIntervalMs() const;
  std::uint64_t maxEventsPerSecond() const;
  std::uint64_t queueLimit() const;
  bool autoRestart() const;
  std::uint32_t maxRestartAttempts() const;
  std::uint64_t restartBackoffMs() const;
  CollectorMode mode() const;
  CollectorLogLevel logLevel() const;

  std::vector<ConfigValidationIssue> validate() const;
  bool isValid() const;

  void merge(const CollectorConfig& other);
  void reset();

  std::size_t fieldCount() const;

private:
  mutable std::mutex mu_;
  std::unordered_map<std::uint16_t, ConfigValue> values_;

  ConfigValue getRaw(CollectorConfigField field) const;
};

}  // namespace monix::collectors
