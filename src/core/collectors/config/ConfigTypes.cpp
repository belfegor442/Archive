#include "ConfigTypes.hpp"

#include <algorithm>

namespace monix::collectors::config {

const char* ConfigChangeSeverityName(ConfigChangeSeverity s) {
  switch (s) {
    case ConfigChangeSeverity::Critical:  return "Critical";
    case ConfigChangeSeverity::Important: return "Important";
    case ConfigChangeSeverity::Normal:    return "Normal";
    case ConfigChangeSeverity::Ignored:   return "Ignored";
  }
  return "Unknown";
}

std::string ConfigChangeSeverityAction(ConfigChangeSeverity s) {
  switch (s) {
    case ConfigChangeSeverity::Critical:  return "config.critical";
    case ConfigChangeSeverity::Important: return "config.important";
    case ConfigChangeSeverity::Normal:    return "config.normal";
    case ConfigChangeSeverity::Ignored:   return "config.ignored";
  }
  return "unknown";
}

const char* ConfigAreaName(ConfigArea a) {
  switch (a) {
    case ConfigArea::Security:             return "Security";
    case ConfigArea::Firewall:             return "Firewall";
    case ConfigArea::Startup:              return "Startup";
    case ConfigArea::Services:             return "Services";
    case ConfigArea::Drivers:              return "Drivers";
    case ConfigArea::Policies:             return "Policies";
    case ConfigArea::SystemConfiguration:  return "SystemConfiguration";
  }
  return "Unknown";
}

ConfigArea ConfigAreaFromName(const std::string& name) {
  if (name == "Security") return ConfigArea::Security;
  if (name == "Firewall") return ConfigArea::Firewall;
  if (name == "Startup") return ConfigArea::Startup;
  if (name == "Services") return ConfigArea::Services;
  if (name == "Drivers") return ConfigArea::Drivers;
  if (name == "Policies") return ConfigArea::Policies;
  if (name == "SystemConfiguration") return ConfigArea::SystemConfiguration;
  return ConfigArea::SystemConfiguration;
}

const char* ConfigChangeKindName(ConfigChangeKind k) {
  switch (k) {
    case ConfigChangeKind::Added:    return "Added";
    case ConfigChangeKind::Removed:  return "Removed";
    case ConfigChangeKind::Modified: return "Modified";
  }
  return "Unknown";
}

const char* ConfigDetectionOriginName(ConfigDetectionOrigin o) {
  switch (o) {
    case ConfigDetectionOrigin::Registry: return "Registry";
    case ConfigDetectionOrigin::Polling:  return "Polling";
    case ConfigDetectionOrigin::Manual:   return "Manual";
  }
  return "Unknown";
}

bool ConfigScope::isAreaEnabled(ConfigArea area) const {
  if (areas.empty()) return true;
  for (const auto& a : areas) {
    if (a == area) return true;
  }
  return false;
}

bool ConfigScope::isSeverityVisible(ConfigChangeSeverity severity) const {
  if (include_ignored) return true;
  if (severity == ConfigChangeSeverity::Ignored) return false;
  return static_cast<std::uint8_t>(severity) <= static_cast<std::uint8_t>(min_severity);
}

bool ConfigScope::matches(const ConfigChange& change) const {
  return isAreaEnabled(change.area) && isSeverityVisible(change.severity);
}

bool ConfigChange::isValid() const {
  return !key_path.empty();
}

bool ConfigChange::isCritical() const {
  return severity == ConfigChangeSeverity::Critical;
}

bool ConfigChange::isImportant() const {
  return severity == ConfigChangeSeverity::Important;
}

bool ConfigChange::isIgnored() const {
  return severity == ConfigChangeSeverity::Ignored;
}

std::string ConfigChange::summary() const {
  std::string result;
  result += "[" + std::string(ConfigChangeSeverityName(severity)) + "] ";
  result += std::string(ConfigAreaName(area)) + " ";
  result += std::string(ConfigChangeKindName(kind)) + " ";
  result += key_path;
  if (!value_name.empty()) result += "\\" + value_name;
  return result;
}

}  // namespace monix::collectors::config
