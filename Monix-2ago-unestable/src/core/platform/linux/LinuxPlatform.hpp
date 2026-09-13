#pragma once

#include "../PlatformInterfaces.hpp"

namespace monix::platform::linux {

class LinuxProcessProvider : public IProcessProvider {
public:
  PlatformType platform() const override;
  std::vector<ProcessInfo> enumerateProcesses() const override;
  ProcessInfo getProcessInfo(std::uint64_t pid) const override;
  bool isProcessRunning(std::uint64_t pid) const override;
};

class LinuxFileSystemProvider : public IFileSystemProvider {
public:
  PlatformType platform() const override;
  std::vector<FileSystemEntry> listDirectory(const std::string& path) const override;
  bool fileExists(const std::string& path) const override;
  std::uint64_t fileSize(const std::string& path) const override;
  bool isReadable(const std::string& path) const override;
};

}  // namespace monix::platform::linux
