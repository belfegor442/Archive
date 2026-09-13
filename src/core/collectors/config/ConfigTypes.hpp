#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace monix::collectors::config {

using ConfigId = std::uint64_t;

enum class ConfigChangeSeverity : std::uint8_t {
  Critical,
  Important,
  Normal,
  Ignored
};

const char* ConfigChangeSeverityName(ConfigChangeSeverity s);
std::string ConfigChangeSeverityAction(ConfigChangeSeverity s);

enum class ConfigArea : std::uint8_t {
  Security,
  Firewall,
  Startup,
  Services,
  Drivers,
  Policies,
  SystemConfiguration
};

const char* ConfigAreaName(ConfigArea a);
ConfigArea ConfigAreaFromName(const std::string& name);

enum class ConfigChangeKind : std::uint8_t {
  Added,
  Removed,
  Modified
};

const char* ConfigChangeKindName(ConfigChangeKind k);

enum class ConfigDetectionOrigin : std::uint8_t {
  Registry,
  Polling,
  Manual
};

const char* ConfigDetectionOriginName(ConfigDetectionOrigin o);

struct ConfigScope {
  std::vector<ConfigArea> areas;
  ConfigChangeSeverity min_severity = ConfigChangeSeverity::Normal;
  bool include_ignored = false;

  bool isAreaEnabled(ConfigArea area) const;
  bool isSeverityVisible(ConfigChangeSeverity severity) const;
  bool matches(const struct ConfigChange& change) const;
};

struct ConfigChange {
  ConfigId id = 0;
  ConfigArea area = ConfigArea::SystemConfiguration;
  ConfigChangeKind kind = ConfigChangeKind::Modified;
  ConfigChangeSeverity severity = ConfigChangeSeverity::Normal;
  std::string key_path;
  std::string value_name;
  std::string old_value;
  std::string new_value;
  std::string description;
  std::int64_t timestamp_ms = 0;

  bool isValid() const;
  bool isCritical() const;
  bool isImportant() const;
  bool isIgnored() const;
  std::string summary() const;
};

}  // namespace monix::collectors::config
