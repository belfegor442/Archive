#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <cstdint>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace monix {

enum class DataState : std::uint8_t {
  Valid = 0,
  Unavailable = 1,
  Unsupported = 2,
  Stale = 3,
  Error = 4,
};

inline const wchar_t* DataStateName(DataState s) {
  switch (s) {
    case DataState::Valid:       return L"VALID";
    case DataState::Unavailable: return L"UNAVAILABLE";
    case DataState::Unsupported: return L"UNSUPPORTED";
    case DataState::Stale:       return L"STALE";
    case DataState::Error:       return L"ERROR";
  }
  return L"UNKNOWN";
}

enum class ModuleClassification : std::uint8_t {
  Trusted = 0,
  Known = 1,
  SignedUnknown = 2,
  Unsigned = 3,
  Untrusted = 4,
  Anomalous = 5,
  Suspicious = 6,
  Unknown = 7,
};

inline const wchar_t* ModuleClassificationName(ModuleClassification c) {
  switch (c) {
    case ModuleClassification::Trusted:      return L"TRUSTED";
    case ModuleClassification::Known:        return L"KNOWN";
    case ModuleClassification::SignedUnknown: return L"SIGNED_UNKNOWN";
    case ModuleClassification::Unsigned:     return L"UNSIGNED";
    case ModuleClassification::Untrusted:    return L"UNTRUSTED";
    case ModuleClassification::Anomalous:    return L"ANOMALOUS";
    case ModuleClassification::Suspicious:   return L"SUSPICIOUS";
    case ModuleClassification::Unknown:      return L"UNKNOWN";
  }
  return L"UNKNOWN";
}

struct ModuleEvidence {
  std::wstring name;
  std::wstring fullPath;
  std::wstring processName;
  int pid = 0;
  std::uint64_t fileSize = 0;
  std::uint32_t timestamp = 0;
  std::wstring sha256Hash;
  int signatureValid = -1;
  std::wstring signerName;
  std::wstring issuerName;
  std::uint32_t certExpiryDays = 0;
  std::uint16_t machineType = 0;
  int protectionStatus = 0;
  bool isSystemModule = false;
  bool isKnownSafe = false;
  bool hookIndicators = false;
  bool detourIndicators = false;
  bool injectionIndicators = false;
  bool peHeaderTamper = false;
  bool memoryImageMismatch = false;
  std::uint64_t loadTimeNs = 0;
  ModuleClassification classification = ModuleClassification::Unknown;
  std::vector<std::wstring> suspiciousIndicators;
  DataState state = DataState::Valid;
};

struct DriverEvidence {
  std::wstring name;
  std::wstring displayName;
  std::wstring path;
  std::wstring version;
  std::wstring publisher;
  std::uint64_t fileSize = 0;
  std::wstring sha256Hash;
  int signatureValid = -1;
  bool isKernelMode = false;
  bool isBootStart = false;
  bool isSystemStart = false;
  bool isDisabled = false;
  std::uint32_t startType = 0;
  std::uint32_t errorControl = 0;
  DataState state = DataState::Valid;
};

struct ProcessEvidence {
  std::wstring name;
  int pid = 0;
  int parentPid = 0;
  int sessionId = 0;
  std::wstring executablePath;
  std::wstring commandLine;
  std::wstring status;
  std::wstring priority;
  std::uint64_t createTime100ns = 0;
  std::uint64_t exitTime100ns = 0;
  std::uint64_t ramBytes = 0;
  double cpuPct = 0.0;
  int integrityLevel = 0;
  std::wstring integrityLevelName;
  int signatureValid = -1;
  std::wstring signerName;
  std::wstring sha256Hash;
  bool isElevated = false;
  bool isProtected = false;
  bool isWow64 = false;
  DataState state = DataState::Valid;
};

struct HardwareDeviceEvidence {
  std::wstring name;
  std::wstring description;
  std::wstring manufacturer;
  std::wstring instanceId;
  std::wstring deviceId;
  std::wstring classGuid;
  std::wstring className;
  DWORD vendorId = 0;
  DWORD deviceId2 = 0;
  DWORD revisionId = 0;
  DWORD bus = 0;
  DWORD device = 0;
  DWORD function = 0;
  bool present = true;
  DataState state = DataState::Valid;
};

struct NetworkInterfaceEvidence {
  std::wstring name;
  std::wstring description;
  std::wstring macAddress;
  std::set<std::wstring> ipv4Addresses;
  std::set<std::wstring> ipv6Addresses;
  std::set<std::wstring> gateways;
  std::set<std::wstring> dnsServers;
  DWORD operStatus = 0;
  DWORD type = 0;
  DWORD speed = 0;
  DWORD mtu = 0;
  bool isDhcpEnabled = false;
  bool isDnsEnabled = false;
  bool isAdapterEnabled = false;
  bool isVpn = false;
  bool isLoopback = false;
  bool isTunnel = false;
  DataState state = DataState::Valid;
};

struct VolumeEvidence {
  std::wstring mountPoint;
  std::wstring label;
  std::wstring fileSystem;
  std::uint32_t serialNumber = 0;
  DWORD maxComponentLength = 0;
  DWORD fileSystemFlags = 0;
  std::uint64_t totalBytes = 0;
  std::uint64_t freeBytes = 0;
  bool isDirty = false;
  bool isReadOnly = false;
  bool isSystemVolume = false;
  bool isBootVolume = false;
  DataState state = DataState::Valid;
};

struct ServiceEvidence {
  std::wstring name;
  std::wstring displayName;
  std::wstring description;
  std::wstring path;
  std::wstring accountName;
  DWORD currentState = 0;
  DWORD startType = 0;
  DWORD serviceType = 0;
  DWORD errorControl = 0;
  DWORD processId = 0;
  DataState state = DataState::Valid;
};

struct StartupItemEvidence {
  std::wstring name;
  std::wstring command;
  std::wstring location;
  std::wstring user;
  bool isEnabled = true;
  DataState state = DataState::Valid;
};

struct ScheduledTaskEvidence {
  std::wstring name;
  std::wstring path;
  std::wstring author;
  bool isEnabled = false;
  bool lastRunSucceeded = false;
  std::wstring lastRunTime;
  DWORD lastRunResult = 0;
  DataState state = DataState::Valid;
};

struct SecurityStateEvidence {
  int defenderRealTimeProtection = -1;
  int defenderTamperProtection = -1;
  int defenderSignatureAge = -1;
  int defenderAntivirusEnabled = -1;
  int defenderBehaviorMonitoring = -1;
  int defenderOnAccessProtection = -1;
  int defenderScanQuickEnabled = -1;
  int uacEnabled = -1;
  int uacConsentPromptBehavior = -1;
  int firewallEnabled = -1;
  int firewallProfiles = 0;
  int lsassProtectionEnabled = -1;
  int credentialGuardEnabled = -1;
  int codeIntegrityEnabled = -1;
  int debugModeEnabled = -1;
  int testSigningEnabled = -1;
  int secureBootEnabled = -1;
  int hvciEnabled = -1;
  DataState state = DataState::Valid;
};

struct FirmwareEvidence {
  std::wstring biosVendor;
  std::wstring biosVersion;
  std::wstring biosReleaseDate;
  std::uint16_t biosMajorVer = 0;
  std::uint16_t biosMinorVer = 0;
  std::wstring systemManufacturer;
  std::wstring systemProductName;
  std::wstring systemSerialNumber;
  unsigned long smbiosHash = 0;
  unsigned long acpiHash = 0;
  int tpmPresent = 0;
  int tpmReady = 0;
  int tpmVersion = 0;
  std::wstring tpmManufacturerId;
  std::wstring tpmFirmwareVersion;
  int secureBootState = -1;
  int platformSecurityDeviceEnabled = -1;
  DataState state = DataState::Valid;
};

struct SoftwareEnvironmentEvidence {
  std::wstring osVersion;
  std::wstring osBuild;
  std::wstring osEdition;
  std::wstring kernelVersion;
  DWORD majorVersion = 0;
  DWORD minorVersion = 0;
  DWORD buildNumber = 0;
  DWORD platformId = 0;
  int currentSessionId = 0;
  std::uint64_t lastBootTime = 0;
  std::uint64_t systemUptimeMs = 0;
  DataState state = DataState::Valid;
};

struct EnvironmentBaseline {
  std::uint64_t baselineId = 0;
  std::uint64_t createdAtMonotonicNs = 0;
  std::uint64_t createdAtWallMs = 0;
  std::wstring hostname;

  FirmwareEvidence firmware;
  SoftwareEnvironmentEvidence software;
  SecurityStateEvidence security;

  std::vector<DriverEvidence> drivers;
  std::vector<ProcessEvidence> processes;
  std::vector<HardwareDeviceEvidence> pciDevices;
  std::vector<HardwareDeviceEvidence> usbDevices;
  std::vector<HardwareDeviceEvidence> displayDevices;
  std::vector<HardwareDeviceEvidence> audioDevices;
  std::vector<HardwareDeviceEvidence> storageDevices;
  std::vector<HardwareDeviceEvidence> networkAdapters;
  std::vector<NetworkInterfaceEvidence> networkInterfaces;
  std::vector<VolumeEvidence> volumes;
  std::vector<ServiceEvidence> services;
  std::vector<StartupItemEvidence> startupItems;
  std::vector<ScheduledTaskEvidence> scheduledTasks;
  std::vector<ModuleEvidence> modules;

  std::set<std::wstring> driverNameSet;
  std::set<std::wstring> processNameSet;
  std::set<std::wstring> serviceNameSet;
  std::set<std::wstring> startupNameSet;
  std::set<std::wstring> taskNameSet;
  std::set<std::wstring> modulePathSet;

  std::uint64_t totalDriverCount = 0;
  std::uint64_t totalProcessCount = 0;
  std::uint64_t totalServiceCount = 0;
  std::uint64_t totalStartupCount = 0;
  std::uint64_t totalTaskCount = 0;
  std::uint64_t totalModuleCount = 0;

  std::uint64_t contentHash = 0;

  void ComputeContentHash() {
    std::size_t h = 0;
    auto mixString = [&](const std::wstring& s) {
      for (wchar_t c : s) h = h * 131 + static_cast<std::size_t>(c);
      h ^= 0x9e3779b9u + (h << 6) + (h >> 2);
    };
    auto mixUint64 = [&](std::uint64_t v) {
      h ^= std::hash<std::uint64_t>{}(v) + 0x9e3779b9u + (h << 6) + (h >> 2);
    };
    auto mixInt = [&](int v) {
      h ^= std::hash<int>{}(v) + 0x9e3779b9u + (h << 6) + (h >> 2);
    };

    // Firmware
    mixString(firmware.biosVersion);
    mixString(firmware.systemProductName);
    mixString(firmware.biosVendor);
    mixString(firmware.systemSerialNumber);
    mixUint64(firmware.smbiosHash);

    // Software
    mixString(software.osVersion);
    mixString(software.osBuild);
    mixUint64(software.lastBootTime);

    // Security state
    mixInt(security.defenderRealTimeProtection);
    mixInt(security.uacEnabled);
    mixInt(security.firewallEnabled);
    mixInt(security.secureBootEnabled);
    mixInt(security.hvciEnabled);
    mixInt(security.codeIntegrityEnabled);

    // Driver name set
    for (const auto& d : driverNameSet) mixString(d);

    // Service name set
    for (const auto& s : serviceNameSet) mixString(s);

    // Module path set
    for (const auto& m : modulePathSet) mixString(m);

    // Process name set (new — was missing)
    for (const auto& p : processNameSet) mixString(p);

    // Startup item name set (new — was missing)
    for (const auto& s : startupNameSet) mixString(s);

    // Task name set (new — was missing)
    for (const auto& t : taskNameSet) mixString(t);

    // Count-based signatures for vector evidence
    mixUint64(processes.size());
    mixUint64(drivers.size());
    mixUint64(pciDevices.size());
    mixUint64(usbDevices.size());
    mixUint64(networkInterfaces.size());
    mixUint64(volumes.size());
    mixUint64(services.size());
    mixUint64(startupItems.size());
    mixUint64(scheduledTasks.size());
    mixUint64(modules.size());

    // Hash representative entries from large vectors (first 5 + last entry)
    auto hashEvidenceVector = [&](const auto& vec, auto nameExtractor) {
      std::size_t count = std::min(vec.size(), std::size_t(5));
      for (std::size_t i = 0; i < count; ++i) mixString(nameExtractor(vec[i]));
      if (vec.size() > 5) mixString(nameExtractor(vec.back()));
    };
    hashEvidenceVector(processes, [](const ProcessEvidence& e) { return e.name; });
    hashEvidenceVector(services, [](const ServiceEvidence& e) { return e.name; });
    hashEvidenceVector(drivers, [](const DriverEvidence& e) { return e.name; });
    hashEvidenceVector(modules, [](const ModuleEvidence& e) { return e.name; });
    hashEvidenceVector(startupItems, [](const StartupItemEvidence& e) { return e.name; });
    hashEvidenceVector(scheduledTasks, [](const ScheduledTaskEvidence& e) { return e.name; });

    // Network interface names
    for (const auto& ni : networkInterfaces) mixString(ni.name);

    // Volume mount points
    for (const auto& v : volumes) mixString(v.mountPoint);

    contentHash = static_cast<std::uint64_t>(h);
  }
};

struct EnvironmentChange {
  enum class Domain {
    Module, Driver, Process, Service, StartupItem, ScheduledTask,
    Hardware, Firmware, Network, Filesystem, Security, Software,
  };

  enum class ChangeKind {
    Added, Removed, Modified, StateChanged,
  };

  Domain domain = Domain::Module;
  ChangeKind kind = ChangeKind::Added;
  std::wstring entityName;
  std::wstring entityPath;
  std::wstring oldValue;
  std::wstring newValue;
  std::wstring evidence;
  std::wstring processName;
  int pid = 0;
  std::uint64_t timestampNs = 0;
  std::uint64_t baselineId = 0;
  bool significant = true;
  bool logged = false;
};

} // namespace monix
