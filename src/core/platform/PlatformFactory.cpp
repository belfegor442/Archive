#include "PlatformInterfaces.hpp"

#ifdef _WIN32
#include "windows/WindowsPlatform.hpp"
#else
#include "linux/LinuxPlatform.hpp"
#endif

namespace monix::platform {

PlatformType PlatformFactory::currentPlatform() {
#ifdef _WIN32
  return PlatformType::Windows;
#elif defined(__linux__)
  return PlatformType::Linux;
#elif defined(__APPLE__)
  return PlatformType::macOS;
#else
  return PlatformType::Unknown;
#endif
}

std::unique_ptr<IProcessProvider> PlatformFactory::createProcessProvider() {
#ifdef _WIN32
  return std::make_unique<windows::WindowsProcessProvider>();
#else
  return std::make_unique<linux::LinuxProcessProvider>();
#endif
}

std::unique_ptr<IFileSystemProvider> PlatformFactory::createFileSystemProvider() {
#ifdef _WIN32
  return std::make_unique<windows::WindowsFileSystemProvider>();
#else
  return std::make_unique<linux::LinuxFileSystemProvider>();
#endif
}

std::unique_ptr<INetworkProvider> PlatformFactory::createNetworkProvider() {
#ifdef _WIN32
  return std::make_unique<windows::WindowsNetworkProvider>();
#else
  return nullptr;
#endif
}

std::unique_ptr<IUserSessionProvider> PlatformFactory::createUserSessionProvider() {
#ifdef _WIN32
  return std::make_unique<windows::WindowsUserSessionProvider>();
#else
  return nullptr;
#endif
}

PlatformCapabilities PlatformFactory::getCapabilities() {
  PlatformCapabilities caps;
#ifdef _WIN32
  caps.supports_process_enumeration = true;
  caps.supports_file_system_watching = true;
  caps.supports_network_enumeration = true;
  caps.supports_user_sessions = true;
  caps.supports_service_enumeration = true;
  caps.supports_driver_enumeration = true;
  caps.supports_registry = true;
  caps.supports_wmi = true;
#elif defined(__linux__)
  caps.supports_process_enumeration = true;
  caps.supports_file_system_watching = true;
  caps.supports_network_enumeration = true;
  caps.supports_journal = true;
  caps.supports_extended_attributes = true;
#endif
  return caps;
}

}  // namespace monix::platform
