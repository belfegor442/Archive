#include "LinuxPlatform.hpp"

namespace monix::platform::linux {

PlatformType LinuxProcessProvider::platform() const { return PlatformType::Linux; }
std::vector<ProcessInfo> LinuxProcessProvider::enumerateProcesses() const { return {}; }
ProcessInfo LinuxProcessProvider::getProcessInfo(std::uint64_t) const { return ProcessInfo{}; }
bool LinuxProcessProvider::isProcessRunning(std::uint64_t) const { return false; }

PlatformType LinuxFileSystemProvider::platform() const { return PlatformType::Linux; }
std::vector<FileSystemEntry> LinuxFileSystemProvider::listDirectory(const std::string&) const { return {}; }
bool LinuxFileSystemProvider::fileExists(const std::string&) const { return false; }
std::uint64_t LinuxFileSystemProvider::fileSize(const std::string&) const { return 0; }
bool LinuxFileSystemProvider::isReadable(const std::string&) const { return false; }

}  // namespace monix::platform::linux
