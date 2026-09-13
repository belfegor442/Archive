#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "EnvironmentBaseline.hpp"
#include "ModuleDetector.hpp"

namespace monix {

class EnvironmentCollector {
 public:
  EnvironmentCollector();
  ~EnvironmentCollector();

  void CollectFirmware(FirmwareEvidence& out);
  void CollectSoftwareEnvironment(SoftwareEnvironmentEvidence& out);
  void CollectSecurityState(SecurityStateEvidence& out);
  void CollectDrivers(std::vector<DriverEvidence>& out);
  void CollectProcesses(std::vector<ProcessEvidence>& out);
  void CollectServices(std::vector<ServiceEvidence>& out);
  void CollectStartupItems(std::vector<StartupItemEvidence>& out);
  void CollectScheduledTasks(std::vector<ScheduledTaskEvidence>& out);
  void CollectPciDevices(std::vector<HardwareDeviceEvidence>& out);
  void CollectUsbDevices(std::vector<HardwareDeviceEvidence>& out);
  void CollectDisplayDevices(std::vector<HardwareDeviceEvidence>& out);
  void CollectAudioDevices(std::vector<HardwareDeviceEvidence>& out);
  void CollectStorageDevices(std::vector<HardwareDeviceEvidence>& out);
  void CollectNetworkInterfaces(std::vector<NetworkInterfaceEvidence>& out);
  void CollectVolumes(std::vector<VolumeEvidence>& out);

  EnvironmentBaseline CollectFullBaseline();
  void DetectChanges(const EnvironmentBaseline& baseline,
                     std::vector<EnvironmentChange>& outChanges);

  ModuleDetector& GetModuleDetector() { return moduleDetector_; }
  const ModuleDetector& GetModuleDetector() const { return moduleDetector_; }

  bool IsInitialized() const { return initialized_; }

 private:
  void InitializeCom();
  void UninitializeCom();
  bool QueryWmiString(const std::wstring& wmiClass, const std::wstring& property,
                      std::wstring& outValue);
  bool QueryWmiUint32(const std::wstring& wmiClass, const std::wstring& property,
                      DWORD& outValue);
  bool QueryWmiUint64(const std::wstring& wmiClass, const std::wstring& property,
                      std::uint64_t& outValue);

  ModuleDetector moduleDetector_;
  bool comInitialized_ = false;
  bool initialized_ = false;
};

} // namespace monix
