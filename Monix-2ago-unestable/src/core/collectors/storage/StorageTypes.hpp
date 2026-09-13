#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace monix::collectors::storage {

using StorageId = std::uint64_t;

enum class StorageKind : std::uint8_t {
  PhysicalDisk,
  Partition,
  Volume,
  ExternalStorage,
  VirtualStorage,
  Unknown
};

const char* StorageKindName(StorageKind k);
StorageKind StorageKindFromName(const std::string& name);

enum class StorageEventKind : std::uint8_t {
  DiskConnected,
  DiskDisconnected,
  VolumeMounted,
  VolumeUnmounted,
  VolumeChanged
};

const char* StorageEventKindName(StorageEventKind k);
std::string StorageEventKindAction(StorageEventKind k);

enum class StorageDriveType : std::uint8_t {
  Unknown,
  Removable,
  Fixed,
  Remote,
  CDROM,
  RAMDisk
};

const char* StorageDriveTypeName(StorageDriveType t);
StorageDriveType DetectDriveType(const std::string& drive_root);

enum class StorageDetectionOrigin : std::uint8_t {
  WMI,
  DeviceIoControl,
  Manual,
  Polling
};

const char* StorageDetectionOriginName(StorageDetectionOrigin o);

struct DiskIdentity {
  std::string device_path;
  std::string serial;
  std::string model;
  std::string vendor;
  std::string bus_type;
  std::uint64_t total_bytes = 0;
  std::uint32_t sector_size = 0;

  bool hasDevicePath() const;
  bool hasSerial() const;
  std::string summary() const;
};

struct VolumeIdentity {
  std::string drive_letter;
  std::string volume_name;
  std::string volume_serial;
  std::string file_system;
  std::string mount_path;
  std::uint64_t total_bytes = 0;
  std::uint64_t free_bytes = 0;
  std::uint32_t serial_number = 0;

  bool hasDriveLetter() const;
  bool hasMountPath() const;
  std::string summary() const;
};

struct StorageInfo {
  StorageId id = 0;
  StorageKind kind = StorageKind::Unknown;
  StorageDriveType drive_type = StorageDriveType::Unknown;
  DiskIdentity disk;
  VolumeIdentity volume;
  std::string correlated_disk_path;
  std::string correlated_volume_letter;
  bool mounted = false;
  std::int64_t timestamp_ms = 0;

  bool isValid() const;
  bool isDisk() const;
  bool isVolume() const;
};

}  // namespace monix::collectors::storage
