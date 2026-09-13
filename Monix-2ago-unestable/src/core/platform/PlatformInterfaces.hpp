#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace monix::platform {

enum class PlatformType : std::uint8_t {
  Windows,
  Linux,
  macOS,
  Unknown
};

const char* PlatformTypeName(PlatformType pt);

struct ProcessInfo {
  std::uint64_t pid = 0;
  std::string name;
  std::string path;
  std::string command_line;
  std::string user;
  std::uint64_t parent_pid = 0;
  std::int64_t start_time_ms = 0;
  std::size_t memory_bytes = 0;
  std::size_t cpu_percent = 0;

  bool isValid() const;
};

struct FileSystemEntry {
  std::string path;
  std::string name;
  bool is_directory = false;
  bool is_file = false;
  bool is_symlink = false;
  bool is_hidden = false;
  std::uint64_t size_bytes = 0;
  std::int64_t modified_ms = 0;
  std::int64_t created_ms = 0;
  std::string permissions;

  bool isValid() const;
};

struct NetworkInterface {
  std::string name;
  std::string ip_address;
  std::string mac_address;
  bool is_up = false;
  bool is_loopback = false;

  bool isValid() const;
};

struct UserSession {
  std::string session_id;
  std::string user_name;
  std::string domain;
  std::string login_type;
  std::int64_t login_time_ms = 0;
  bool is_active = false;

  bool isValid() const;
};

struct PlatformCapabilities {
  bool supports_process_enumeration = false;
  bool supports_file_system_watching = false;
  bool supports_network_enumeration = false;
  bool supports_user_sessions = false;
  bool supports_service_enumeration = false;
  bool supports_driver_enumeration = false;
  bool supports_registry = false;
  bool supports_wmi = false;
  bool supports_journal = false;
  bool supports_extended_attributes = false;

  std::string summary() const;
};

class IProcessProvider {
public:
  virtual ~IProcessProvider() = default;
  virtual PlatformType platform() const = 0;
  virtual std::vector<ProcessInfo> enumerateProcesses() const = 0;
  virtual ProcessInfo getProcessInfo(std::uint64_t pid) const = 0;
  virtual bool isProcessRunning(std::uint64_t pid) const = 0;
};

class IFileSystemProvider {
public:
  virtual ~IFileSystemProvider() = default;
  virtual PlatformType platform() const = 0;
  virtual std::vector<FileSystemEntry> listDirectory(const std::string& path) const = 0;
  virtual bool fileExists(const std::string& path) const = 0;
  virtual std::uint64_t fileSize(const std::string& path) const = 0;
  virtual bool isReadable(const std::string& path) const = 0;
};

class INetworkProvider {
public:
  virtual ~INetworkProvider() = default;
  virtual PlatformType platform() const = 0;
  virtual std::vector<NetworkInterface> enumerateInterfaces() const = 0;
};

class IUserSessionProvider {
public:
  virtual ~IUserSessionProvider() = default;
  virtual PlatformType platform() const = 0;
  virtual std::vector<UserSession> enumerateSessions() const = 0;
};

class PlatformFactory {
public:
  static std::unique_ptr<IProcessProvider> createProcessProvider();
  static std::unique_ptr<IFileSystemProvider> createFileSystemProvider();
  static std::unique_ptr<INetworkProvider> createNetworkProvider();
  static std::unique_ptr<IUserSessionProvider> createUserSessionProvider();
  static PlatformCapabilities getCapabilities();
  static PlatformType currentPlatform();
};

}  // namespace monix::platform
