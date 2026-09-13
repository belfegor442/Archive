#include "StorageTypes.hpp"

#include <algorithm>
#include <cctype>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace monix::collectors::storage {

const char* StorageKindName(StorageKind k) {
  switch (k) {
    case StorageKind::PhysicalDisk:     return "PhysicalDisk";
    case StorageKind::Partition:        return "Partition";
    case StorageKind::Volume:           return "Volume";
    case StorageKind::ExternalStorage:  return "ExternalStorage";
    case StorageKind::VirtualStorage:   return "VirtualStorage";
    case StorageKind::Unknown:          return "Unknown";
  }
  return "Unknown";
}

StorageKind StorageKindFromName(const std::string& name) {
  std::string lower = name;
  std::transform(lower.begin(), lower.end(), lower.begin(),
    [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

  if (lower == "physicaldisk" || lower == "physical_disk") return StorageKind::PhysicalDisk;
  if (lower == "partition") return StorageKind::Partition;
  if (lower == "volume") return StorageKind::Volume;
  if (lower == "externalstorage" || lower == "external_storage") return StorageKind::ExternalStorage;
  if (lower == "virtualstorage" || lower == "virtual_storage") return StorageKind::VirtualStorage;
  return StorageKind::Unknown;
}

const char* StorageEventKindName(StorageEventKind k) {
  switch (k) {
    case StorageEventKind::DiskConnected:    return "DiskConnected";
    case StorageEventKind::DiskDisconnected: return "DiskDisconnected";
    case StorageEventKind::VolumeMounted:    return "VolumeMounted";
    case StorageEventKind::VolumeUnmounted:  return "VolumeUnmounted";
    case StorageEventKind::VolumeChanged:    return "VolumeChanged";
  }
  return "Unknown";
}

std::string StorageEventKindAction(StorageEventKind k) {
  switch (k) {
    case StorageEventKind::DiskConnected:    return "disk.connected";
    case StorageEventKind::DiskDisconnected: return "disk.disconnected";
    case StorageEventKind::VolumeMounted:    return "volume.mounted";
    case StorageEventKind::VolumeUnmounted:  return "volume.unmounted";
    case StorageEventKind::VolumeChanged:    return "volume.changed";
  }
  return "unknown";
}

const char* StorageDriveTypeName(StorageDriveType t) {
  switch (t) {
    case StorageDriveType::Unknown:   return "Unknown";
    case StorageDriveType::Removable: return "Removable";
    case StorageDriveType::Fixed:     return "Fixed";
    case StorageDriveType::Remote:    return "Remote";
    case StorageDriveType::CDROM:     return "CDROM";
    case StorageDriveType::RAMDisk:   return "RAMDisk";
  }
  return "Unknown";
}

StorageDriveType DetectDriveType(const std::string& drive_root) {
  std::wstring wdrive(drive_root.begin(), drive_root.end());
  UINT type = GetDriveTypeW(wdrive.c_str());
  switch (type) {
    case DRIVE_REMOVABLE: return StorageDriveType::Removable;
    case DRIVE_FIXED:     return StorageDriveType::Fixed;
    case DRIVE_REMOTE:    return StorageDriveType::Remote;
    case DRIVE_CDROM:     return StorageDriveType::CDROM;
    case DRIVE_RAMDISK:   return StorageDriveType::RAMDisk;
    default:              return StorageDriveType::Unknown;
  }
}

const char* StorageDetectionOriginName(StorageDetectionOrigin o) {
  switch (o) {
    case StorageDetectionOrigin::WMI:              return "WMI";
    case StorageDetectionOrigin::DeviceIoControl:  return "DeviceIoControl";
    case StorageDetectionOrigin::Manual:           return "Manual";
    case StorageDetectionOrigin::Polling:          return "Polling";
  }
  return "Unknown";
}

bool DiskIdentity::hasDevicePath() const {
  return !device_path.empty();
}

bool DiskIdentity::hasSerial() const {
  return !serial.empty();
}

std::string DiskIdentity::summary() const {
  if (!model.empty() && !vendor.empty()) {
    return vendor + " " + model;
  }
  if (!model.empty()) return model;
  if (!device_path.empty()) return device_path;
  return "Unknown Disk";
}

bool VolumeIdentity::hasDriveLetter() const {
  return !drive_letter.empty();
}

bool VolumeIdentity::hasMountPath() const {
  return !mount_path.empty();
}

std::string VolumeIdentity::summary() const {
  if (!drive_letter.empty()) {
    std::string result = drive_letter;
    if (!volume_name.empty()) result += " (" + volume_name + ")";
    return result;
  }
  if (!mount_path.empty()) return mount_path;
  return "Unknown Volume";
}

bool StorageInfo::isValid() const {
  return id != 0 || disk.hasDevicePath() || volume.hasDriveLetter();
}

bool StorageInfo::isDisk() const {
  return kind == StorageKind::PhysicalDisk || kind == StorageKind::Partition;
}

bool StorageInfo::isVolume() const {
  return kind == StorageKind::Volume || kind == StorageKind::ExternalStorage;
}

}  // namespace monix::collectors::storage
