#include "PlatformInterfaces.hpp"

namespace monix::platform {

const char* PlatformTypeName(PlatformType pt) {
  switch (pt) {
    case PlatformType::Windows: return "Windows";
    case PlatformType::Linux:   return "Linux";
    case PlatformType::macOS:   return "macOS";
    case PlatformType::Unknown: return "Unknown";
  }
  return "Unknown";
}

bool ProcessInfo::isValid() const {
  return pid > 0 && !name.empty();
}

bool FileSystemEntry::isValid() const {
  return !path.empty();
}

bool NetworkInterface::isValid() const {
  return !name.empty();
}

bool UserSession::isValid() const {
  return !session_id.empty();
}

std::string PlatformCapabilities::summary() const {
  std::string s;
  if (supports_process_enumeration) s += "process ";
  if (supports_file_system_watching) s += "fs_watch ";
  if (supports_network_enumeration) s += "network ";
  if (supports_user_sessions) s += "sessions ";
  if (supports_service_enumeration) s += "services ";
  if (supports_driver_enumeration) s += "drivers ";
  if (supports_registry) s += "registry ";
  if (supports_wmi) s += "wmi ";
  if (supports_journal) s += "journal ";
  if (supports_extended_attributes) s += "xattr ";
  return s;
}

}  // namespace monix::platform
