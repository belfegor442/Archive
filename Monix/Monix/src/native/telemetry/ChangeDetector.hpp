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
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "EnvironmentBaseline.hpp"
#include "EnvironmentCollector.hpp"

namespace monix {

struct ChangeDetectorConfig {
  int environmentCheckIntervalSamples = 20;
  int moduleCheckIntervalSamples = 5;
  int driverCheckIntervalSamples = 10;
  int processCheckIntervalSamples = 3;
  int serviceCheckIntervalSamples = 20;
  int startupCheckIntervalSamples = 20;
  int securityCheckIntervalSamples = 15;
  int networkCheckIntervalSamples = 5;
  int filesystemCheckIntervalSamples = 10;
  int firmwareCheckIntervalSamples = 50;
  bool enableModuleDeepScan = true;
  bool enableDriverSignatureCheck = false;
  bool enableProcessIntegrityCheck = true;
};

struct ChangeDetectionState {
  std::uint64_t baselineId = 0;
  std::uint64_t lastCheckSample = 0;
  std::uint64_t lastEnvironmentCheckSample = 0;
  std::uint64_t lastModuleCheckSample = 0;
  std::uint64_t lastDriverCheckSample = 0;
  std::uint64_t lastProcessCheckSample = 0;
  std::uint64_t lastServiceCheckSample = 0;
  std::uint64_t lastStartupCheckSample = 0;
  std::uint64_t lastSecurityCheckSample = 0;
  std::uint64_t lastNetworkCheckSample = 0;
  std::uint64_t lastFilesystemCheckSample = 0;
  std::uint64_t lastFirmwareCheckSample = 0;

  int totalChangesDetected = 0;
  int significantChangesDetected = 0;
  int moduleChangesDetected = 0;
  int driverChangesDetected = 0;
  int processChangesDetected = 0;
  int serviceChangesDetected = 0;
  int startupChangesDetected = 0;
  int securityChangesDetected = 0;
  int networkChangesDetected = 0;
  int filesystemChangesDetected = 0;
  int firmwareChangesDetected = 0;

  std::set<std::wstring> knownModulePaths;
  std::set<std::wstring> knownDriverNames;
  std::set<std::wstring> knownServiceNames;
  std::set<std::wstring> knownStartupNames;
  std::set<int> knownProcessPids;
  std::set<std::wstring> knownProcessNames;
  std::map<std::wstring, std::uint64_t> moduleFirstSeenNs;
  std::map<std::wstring, std::uint64_t> driverFirstSeenNs;
  std::map<int, std::uint64_t> processFirstSeenNs;
  std::map<std::wstring, std::uint64_t> serviceFirstSeenNs;
  std::map<std::wstring, std::uint64_t> startupFirstSeenNs;
};

class ChangeDetector {
 public:
  explicit ChangeDetector(const ChangeDetectorConfig& config = {});

  void Initialize(EnvironmentCollector& collector);
  void OnSnapshotCollected(std::uint64_t currentSample);
  std::vector<EnvironmentChange> DetectChanges(EnvironmentCollector& collector);

  void SetBaseline(const EnvironmentBaseline& baseline);
  const EnvironmentBaseline& GetBaseline() const { return baseline_; }
  bool IsBaselineReady() const { return baselineReady_; }

  const ChangeDetectionState& GetState() const { return state_; }
  ChangeDetectionState& GetMutableState() { return state_; }

  int GetTotalChangesDetected() const { return state_.totalChangesDetected; }
  int GetSignificantChangesDetected() const { return state_.significantChangesDetected; }

  void AddChangeCallback(std::function<void(const EnvironmentChange&)> callback) {
    changeCallbacks_.push_back(std::move(callback));
  }

  void RecordModuleObservation(const std::wstring& modulePath, std::uint64_t timestampNs);
  void RecordDriverObservation(const std::wstring& driverName, std::uint64_t timestampNs);
  void RecordProcessObservation(int pid, const std::wstring& processName, std::uint64_t timestampNs);
  void RecordServiceObservation(const std::wstring& serviceName, std::uint64_t timestampNs);
  void RecordStartupObservation(const std::wstring& itemName, std::uint64_t timestampNs);

  bool IsModuleFirstSeen(const std::wstring& modulePath) const;
  bool IsDriverFirstSeen(const std::wstring& driverName) const;
  bool IsProcessFirstSeen(int pid) const;
  bool IsServiceFirstSeen(const std::wstring& serviceName) const;
  bool IsStartupFirstSeen(const std::wstring& itemName) const;

  std::uint64_t GetModuleFirstSeenTime(const std::wstring& modulePath) const;
  std::uint64_t GetDriverFirstSeenTime(const std::wstring& driverName) const;
  std::uint64_t GetProcessFirstSeenTime(int pid) const;

 private:
  void EmitChange(const EnvironmentChange& change);
  bool ShouldCheckDomain(EnvironmentChange::Domain domain, std::uint64_t currentSample) const;
  void UpdateStateFromBaseline();

  ChangeDetectorConfig config_;
  ChangeDetectionState state_;
  EnvironmentBaseline baseline_;
  bool baselineReady_ = false;
  std::vector<std::function<void(const EnvironmentChange&)>> changeCallbacks_;
};

} // namespace monix
