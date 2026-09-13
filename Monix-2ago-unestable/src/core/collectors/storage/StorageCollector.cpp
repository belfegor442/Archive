#include "StorageCollector.hpp"

#include <algorithm>
#include <chrono>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace monix::collectors::storage {

StorageCollectorConfig StorageCollectorConfig::defaults() {
  StorageCollectorConfig cfg;
  cfg.track_physical_disk = true;
  cfg.track_partitions = true;
  cfg.track_volumes = true;
  cfg.track_external = true;
  cfg.track_virtual = true;
  cfg.auto_enumerate = true;
  cfg.max_events = 10000;
  return cfg;
}

StorageCollector::StorageCollector() = default;

StorageCollector::~StorageCollector() {
  stop();
}

bool StorageCollector::start(StorageCollectorConfig config) {
  if (running_) return false;
  config_ = std::move(config);
  running_ = true;
  events_emitted_ = 0;
  tracked_disks_.clear();
  tracked_volumes_.clear();
  event_log_.clear();

  if (config_.auto_enumerate) {
    auto disks = enumerateDisks();
    auto volumes = enumerateVolumes();

    std::lock_guard<std::mutex> lock(mu_);
    for (const auto& d : disks) {
      StorageId id = generateId(d.disk, d.volume);
      if (id != 0 && tracked_disks_.size() < config_.max_events) {
        tracked_disks_[id] = d;
      }
    }
    for (const auto& v : volumes) {
      StorageId id = generateId(v.disk, v.volume);
      if (id != 0 && tracked_volumes_.size() < config_.max_events) {
        tracked_volumes_[id] = v;
      }
    }
  }

  return true;
}

bool StorageCollector::stop() {
  if (!running_) return false;
  running_ = false;
  tracked_disks_.clear();
  tracked_volumes_.clear();
  event_log_.clear();
  return true;
}

bool StorageCollector::isRunning() const {
  return running_;
}

void StorageCollector::setCallback(StorageCallback callback) {
  callback_ = std::move(callback);
}

void StorageCollector::reportDiskConnected(const StorageInfo& info, StorageDetectionOrigin origin) {
  if (!running_) return;
  if (!shouldTrack(info.kind)) return;
  if (isDuplicate(info, StorageEventKind::DiskConnected)) return;

  StorageInfo enriched = info;
  if (enriched.id == 0) {
    enriched.id = generateId(enriched.disk, enriched.volume);
  }
  if (enriched.timestamp_ms == 0) {
    enriched.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count();
  }

  {
    std::lock_guard<std::mutex> lock(mu_);
    if (enriched.id != 0 && tracked_disks_.size() < config_.max_events) {
      tracked_disks_[enriched.id] = enriched;
    }
  }

  emit(StorageEventKind::DiskConnected, origin, enriched);
}

void StorageCollector::reportDiskDisconnected(const StorageInfo& info, StorageDetectionOrigin origin) {
  if (!running_) return;
  if (!shouldTrack(info.kind)) return;
  if (isDuplicate(info, StorageEventKind::DiskDisconnected)) return;

  StorageInfo enriched = info;
  if (enriched.id == 0) {
    enriched.id = generateId(enriched.disk, enriched.volume);
  }
  if (enriched.timestamp_ms == 0) {
    enriched.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count();
  }

  {
    std::lock_guard<std::mutex> lock(mu_);
    tracked_disks_.erase(enriched.id);
  }

  emit(StorageEventKind::DiskDisconnected, origin, enriched);
}

void StorageCollector::reportVolumeMounted(const StorageInfo& info, StorageDetectionOrigin origin) {
  if (!running_) return;
  if (!shouldTrack(info.kind)) return;
  if (isDuplicate(info, StorageEventKind::VolumeMounted)) return;

  StorageInfo enriched = info;
  enriched.mounted = true;
  if (enriched.id == 0) {
    enriched.id = generateId(enriched.disk, enriched.volume);
  }
  if (enriched.timestamp_ms == 0) {
    enriched.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count();
  }

  {
    std::lock_guard<std::mutex> lock(mu_);
    if (enriched.id != 0 && tracked_volumes_.size() < config_.max_events) {
      tracked_volumes_[enriched.id] = enriched;
    }
  }

  emit(StorageEventKind::VolumeMounted, origin, enriched);
}

void StorageCollector::reportVolumeUnmounted(const StorageInfo& info, StorageDetectionOrigin origin) {
  if (!running_) return;
  if (!shouldTrack(info.kind)) return;
  if (isDuplicate(info, StorageEventKind::VolumeUnmounted)) return;

  StorageInfo enriched = info;
  enriched.mounted = false;
  if (enriched.id == 0) {
    enriched.id = generateId(enriched.disk, enriched.volume);
  }
  if (enriched.timestamp_ms == 0) {
    enriched.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count();
  }

  {
    std::lock_guard<std::mutex> lock(mu_);
    tracked_volumes_.erase(enriched.id);
  }

  emit(StorageEventKind::VolumeUnmounted, origin, enriched);
}

void StorageCollector::reportVolumeChanged(const StorageInfo& info, StorageDetectionOrigin origin) {
  if (!running_) return;
  if (!shouldTrack(info.kind)) return;
  if (isDuplicate(info, StorageEventKind::VolumeChanged)) return;

  StorageInfo enriched = info;
  if (enriched.id == 0) {
    enriched.id = generateId(enriched.disk, enriched.volume);
  }
  if (enriched.timestamp_ms == 0) {
    enriched.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count();
  }

  {
    std::lock_guard<std::mutex> lock(mu_);
    if (enriched.id != 0) {
      tracked_volumes_[enriched.id] = enriched;
    }
  }

  emit(StorageEventKind::VolumeChanged, origin, enriched);
}

std::size_t StorageCollector::eventsEmitted() const {
  return events_emitted_;
}

std::size_t StorageCollector::disksTracked() const {
  return tracked_disks_.size();
}

std::size_t StorageCollector::volumesTracked() const {
  return tracked_volumes_.size();
}

std::vector<StorageInfo> StorageCollector::currentDisks() const {
  std::lock_guard<std::mutex> lock(mu_);
  std::vector<StorageInfo> result;
  for (const auto& [id, info] : tracked_disks_) {
    result.push_back(info);
  }
  return result;
}

std::vector<StorageInfo> StorageCollector::currentVolumes() const {
  std::lock_guard<std::mutex> lock(mu_);
  std::vector<StorageInfo> result;
  for (const auto& [id, info] : tracked_volumes_) {
    result.push_back(info);
  }
  return result;
}

std::vector<StorageInfo> StorageCollector::recentEvents(std::size_t count) const {
  std::lock_guard<std::mutex> lock(mu_);
  std::size_t start = event_log_.size() > count ? event_log_.size() - count : 0;
  return std::vector<StorageInfo>(event_log_.begin() + start, event_log_.end());
}

bool StorageCollector::isDiskTracked(const std::string& device_path) const {
  std::lock_guard<std::mutex> lock(mu_);
  for (const auto& [id, info] : tracked_disks_) {
    if (info.disk.device_path == device_path) return true;
  }
  return false;
}

bool StorageCollector::isVolumeMounted(const std::string& drive_letter) const {
  std::lock_guard<std::mutex> lock(mu_);
  for (const auto& [id, info] : tracked_volumes_) {
    if (info.volume.drive_letter == drive_letter && info.mounted) return true;
  }
  return false;
}

std::vector<StorageInfo> StorageCollector::enumerateDisks() const {
  std::vector<StorageInfo> result;

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
  HANDLE hDevice = CreateFileW(
    L"\\\\.\\PhysicalDrive0",
    0, FILE_SHARE_READ | FILE_SHARE_WRITE,
    nullptr, OPEN_EXISTING, 0, nullptr);

  if (hDevice != INVALID_HANDLE_VALUE) {
    StorageInfo info;
    info.kind = StorageKind::PhysicalDisk;
    info.drive_type = StorageDriveType::Fixed;
    info.disk.device_path = "\\\\.\\PhysicalDrive0";
    info.mounted = true;
    info.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count();
    info.id = generateId(info.disk, info.volume);
    result.push_back(std::move(info));
    CloseHandle(hDevice);
  }

  DWORD drives = GetLogicalDrives();
  for (int i = 0; i < 26; i++) {
    if (drives & (1 << i)) {
      char letter = 'A' + i;
      std::string root = std::string(1, letter) + ":\\";
      std::wstring wroot(root.begin(), root.end());

      UINT type = GetDriveTypeW(wroot.c_str());
      StorageKind kind = StorageKind::Volume;
      StorageDriveType dtype = StorageDriveType::Unknown;
      if (type == DRIVE_REMOVABLE) {
        dtype = StorageDriveType::Removable;
        kind = StorageKind::ExternalStorage;
      } else if (type == DRIVE_FIXED) {
        dtype = StorageDriveType::Fixed;
      } else if (type == DRIVE_REMOTE) {
        dtype = StorageDriveType::Remote;
      } else if (type == DRIVE_CDROM) {
        dtype = StorageDriveType::CDROM;
      }

      DWORD serial = 0, max_comp = 0, flags = 0;
      wchar_t fs_name[256]{};
      if (GetVolumeInformationW(wroot.c_str(), nullptr, 0, &serial, &max_comp, &flags, fs_name, 256)) {
        StorageInfo info;
        info.kind = kind;
        info.drive_type = dtype;
        info.volume.drive_letter = std::string(1, letter);
        info.volume.serial_number = serial;
        info.volume.file_system = std::string(reinterpret_cast<const char*>(fs_name));
        info.mounted = true;
        info.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::system_clock::now().time_since_epoch()).count();
        info.id = generateId(info.disk, info.volume);
        result.push_back(std::move(info));
      }
    }
  }

  return result;
}

std::vector<StorageInfo> StorageCollector::enumerateVolumes() const {
  std::vector<StorageInfo> result;
  DWORD drives = GetLogicalDrives();
  for (int i = 0; i < 26; i++) {
    if (drives & (1 << i)) {
      char letter = 'A' + i;
      std::string root = std::string(1, letter) + ":\\";
      std::wstring wroot(root.begin(), root.end());

      UINT type = GetDriveTypeW(wroot.c_str());
      StorageKind kind = StorageKind::Volume;
      StorageDriveType dtype = StorageDriveType::Unknown;
      if (type == DRIVE_REMOVABLE) {
        dtype = StorageDriveType::Removable;
        kind = StorageKind::ExternalStorage;
      } else if (type == DRIVE_FIXED) {
        dtype = StorageDriveType::Fixed;
      } else if (type == DRIVE_REMOTE) {
        dtype = StorageDriveType::Remote;
      } else if (type == DRIVE_CDROM) {
        dtype = StorageDriveType::CDROM;
      }

      DWORD serial = 0, max_comp = 0, flags = 0;
      wchar_t vol_name[256]{};
      wchar_t fs_name[256]{};
      ULARGE_INTEGER free_bytes{}, total_bytes{}, total_free{};
      if (GetVolumeInformationW(wroot.c_str(), vol_name, 256, &serial, &max_comp, &flags, fs_name, 256)) {
        GetDiskFreeSpaceExW(wroot.c_str(), nullptr, &total_bytes, &free_bytes);

        StorageInfo info;
        info.kind = kind;
        info.drive_type = dtype;
        info.volume.drive_letter = std::string(1, letter);
        info.volume.serial_number = serial;
        info.volume.file_system = std::string(reinterpret_cast<const char*>(fs_name));
        int sz = WideCharToMultiByte(CP_UTF8, 0, vol_name, -1, nullptr, 0, nullptr, nullptr);
        if (sz > 0) {
          info.volume.volume_name.resize(static_cast<std::size_t>(sz) - 1);
          WideCharToMultiByte(CP_UTF8, 0, vol_name, -1, info.volume.volume_name.data(), sz, nullptr, nullptr);
        }
        info.volume.total_bytes = total_bytes.QuadPart;
        info.volume.free_bytes = free_bytes.QuadPart;
        info.mounted = true;
        info.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::system_clock::now().time_since_epoch()).count();
        info.id = generateId(info.disk, info.volume);
        result.push_back(std::move(info));
      }
    }
  }
  return result;
}

bool StorageCollector::shouldTrack(StorageKind kind) const {
  switch (kind) {
    case StorageKind::PhysicalDisk:     return config_.track_physical_disk;
    case StorageKind::Partition:        return config_.track_partitions;
    case StorageKind::Volume:           return config_.track_volumes;
    case StorageKind::ExternalStorage:  return config_.track_external;
    case StorageKind::VirtualStorage:   return config_.track_virtual;
    case StorageKind::Unknown:          return false;
  }
  return false;
}

void StorageCollector::emit(StorageEventKind kind, StorageDetectionOrigin origin, const StorageInfo& info) {
  events_emitted_++;
  {
    std::lock_guard<std::mutex> lock(mu_);
    if (event_log_.size() >= config_.max_events) {
      event_log_.erase(event_log_.begin());
    }
    event_log_.push_back(info);
  }
  if (callback_) {
    callback_(kind, origin, info);
  }
}

bool StorageCollector::isDuplicate(const StorageInfo& info, StorageEventKind kind) {
  std::string key;
  key.reserve(128);
  key += std::to_string(static_cast<int>(kind));
  key += ":";
  if (!info.disk.device_path.empty()) {
    key += "d:" + info.disk.device_path;
  } else if (!info.volume.drive_letter.empty()) {
    key += "v:" + info.volume.drive_letter;
  } else if (info.id != 0) {
    key += "i:" + std::to_string(info.id);
  } else {
    return false;
  }

  auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::system_clock::now().time_since_epoch()).count();
  std::string dedup_key = key + "@" + std::to_string(now / 3000);

  std::lock_guard<std::mutex> lock(mu_);
  if (dedup_key_set_.count(dedup_key) > 0) {
    return true;
  }
  dedup_key_set_.insert(dedup_key);
  return false;
}

StorageId StorageCollector::generateId(const DiskIdentity& disk, const VolumeIdentity& vol) const {
  if (!disk.device_path.empty()) {
    std::hash<std::string> hasher;
    return static_cast<StorageId>(hasher(disk.device_path));
  }
  if (!vol.drive_letter.empty()) {
    return static_cast<StorageId>(vol.drive_letter[0]) << 32 | vol.serial_number;
  }
  return 0;
}

}  // namespace monix::collectors::storage
