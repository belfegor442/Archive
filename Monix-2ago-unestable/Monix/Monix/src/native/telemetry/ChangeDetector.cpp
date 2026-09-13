#include "ChangeDetector.hpp"

#include <algorithm>
#include <set>
#include <string>
#include <vector>

namespace monix {

ChangeDetector::ChangeDetector(const ChangeDetectorConfig& config)
    : config_(config) {}

void ChangeDetector::Initialize(EnvironmentCollector& collector) {
  EnvironmentBaseline baseline = collector.CollectFullBaseline();
  SetBaseline(baseline);
}

void ChangeDetector::SetBaseline(const EnvironmentBaseline& baseline) {
  baseline_ = baseline;
  baselineReady_ = true;
  state_.baselineId = baseline.baselineId;
  UpdateStateFromBaseline();
}

void ChangeDetector::UpdateStateFromBaseline() {
  state_.knownModulePaths.clear();
  state_.knownDriverNames.clear();
  state_.knownServiceNames.clear();
  state_.knownStartupNames.clear();
  state_.knownProcessPids.clear();
  state_.knownProcessNames.clear();

  for (const auto& mod : baseline_.modules) {
    state_.knownModulePaths.insert(mod.fullPath);
    state_.moduleFirstSeenNs[mod.fullPath] = baseline_.createdAtMonotonicNs;
  }

  for (const auto& drv : baseline_.drivers) {
    state_.knownDriverNames.insert(drv.name);
    state_.driverFirstSeenNs[drv.name] = baseline_.createdAtMonotonicNs;
  }

  for (const auto& svc : baseline_.services) {
    state_.knownServiceNames.insert(svc.name);
    state_.serviceFirstSeenNs[svc.name] = baseline_.createdAtMonotonicNs;
  }

  for (const auto& item : baseline_.startupItems) {
    state_.knownStartupNames.insert(item.name);
    state_.startupFirstSeenNs[item.name] = baseline_.createdAtMonotonicNs;
  }

  for (const auto& proc : baseline_.processes) {
    state_.knownProcessPids.insert(proc.pid);
    state_.knownProcessNames.insert(proc.name);
    state_.processFirstSeenNs[proc.pid] = baseline_.createdAtMonotonicNs;
  }
}

bool ChangeDetector::ShouldCheckDomain(EnvironmentChange::Domain domain, std::uint64_t currentSample) const {
  switch (domain) {
    case EnvironmentChange::Domain::Module:
      return (currentSample - state_.lastModuleCheckSample) >= config_.moduleCheckIntervalSamples;
    case EnvironmentChange::Domain::Driver:
      return (currentSample - state_.lastDriverCheckSample) >= config_.driverCheckIntervalSamples;
    case EnvironmentChange::Domain::Process:
      return (currentSample - state_.lastProcessCheckSample) >= config_.processCheckIntervalSamples;
    case EnvironmentChange::Domain::Service:
      return (currentSample - state_.lastServiceCheckSample) >= config_.serviceCheckIntervalSamples;
    case EnvironmentChange::Domain::StartupItem:
      return (currentSample - state_.lastStartupCheckSample) >= config_.startupCheckIntervalSamples;
    case EnvironmentChange::Domain::Security:
      return (currentSample - state_.lastSecurityCheckSample) >= config_.securityCheckIntervalSamples;
    case EnvironmentChange::Domain::Network:
      return (currentSample - state_.lastNetworkCheckSample) >= config_.networkCheckIntervalSamples;
    case EnvironmentChange::Domain::Filesystem:
      return (currentSample - state_.lastFilesystemCheckSample) >= config_.filesystemCheckIntervalSamples;
    case EnvironmentChange::Domain::Firmware:
      return (currentSample - state_.lastFirmwareCheckSample) >= config_.firmwareCheckIntervalSamples;
    case EnvironmentChange::Domain::Hardware:
    case EnvironmentChange::Domain::Software:
      return (currentSample - state_.lastEnvironmentCheckSample) >= config_.environmentCheckIntervalSamples;
  }
  return false;
}

void ChangeDetector::OnSnapshotCollected(std::uint64_t currentSample) {
  state_.lastCheckSample = currentSample;
}

std::vector<EnvironmentChange> ChangeDetector::DetectChanges(EnvironmentCollector& collector) {
  std::vector<EnvironmentChange> allChanges;

  if (ShouldCheckDomain(EnvironmentChange::Domain::Driver, state_.lastCheckSample)) {
    std::vector<DriverEvidence> currentDrivers;
    collector.CollectDrivers(currentDrivers);

    std::set<std::wstring> currentDriverNames;
    for (const auto& d : currentDrivers) currentDriverNames.insert(d.name);

    for (const auto& name : currentDriverNames) {
      if (state_.knownDriverNames.find(name) == state_.knownDriverNames.end()) {
        EnvironmentChange change;
        change.domain = EnvironmentChange::Domain::Driver;
        change.kind = EnvironmentChange::ChangeKind::Added;
        change.entityName = name;
        change.evidence = L"driver_loaded";
        change.significant = true;
        EmitChange(change);
        allChanges.push_back(std::move(change));
        ++state_.driverChangesDetected;
        ++state_.significantChangesDetected;
      }
    }

    for (const auto& name : state_.knownDriverNames) {
      if (currentDriverNames.find(name) == currentDriverNames.end()) {
        EnvironmentChange change;
        change.domain = EnvironmentChange::Domain::Driver;
        change.kind = EnvironmentChange::ChangeKind::Removed;
        change.entityName = name;
        change.evidence = L"driver_unloaded";
        change.significant = true;
        EmitChange(change);
        allChanges.push_back(std::move(change));
        ++state_.driverChangesDetected;
        ++state_.significantChangesDetected;
      }
    }

    state_.lastDriverCheckSample = state_.lastCheckSample;
  }

  if (ShouldCheckDomain(EnvironmentChange::Domain::Service, state_.lastCheckSample)) {
    std::vector<ServiceEvidence> currentServices;
    collector.CollectServices(currentServices);

    std::set<std::wstring> currentServiceNames;
    for (const auto& s : currentServices) currentServiceNames.insert(s.name);

    for (const auto& name : currentServiceNames) {
      if (state_.knownServiceNames.find(name) == state_.knownServiceNames.end()) {
        EnvironmentChange change;
        change.domain = EnvironmentChange::Domain::Service;
        change.kind = EnvironmentChange::ChangeKind::Added;
        change.entityName = name;
        change.evidence = L"service_created";
        change.significant = true;
        EmitChange(change);
        allChanges.push_back(std::move(change));
        ++state_.serviceChangesDetected;
        ++state_.significantChangesDetected;
      }
    }

    for (const auto& name : state_.knownServiceNames) {
      if (currentServiceNames.find(name) == currentServiceNames.end()) {
        EnvironmentChange change;
        change.domain = EnvironmentChange::Domain::Service;
        change.kind = EnvironmentChange::ChangeKind::Removed;
        change.entityName = name;
        change.evidence = L"service_removed";
        change.significant = true;
        EmitChange(change);
        allChanges.push_back(std::move(change));
        ++state_.serviceChangesDetected;
        ++state_.significantChangesDetected;
      }
    }

    state_.lastServiceCheckSample = state_.lastCheckSample;
  }

  if (ShouldCheckDomain(EnvironmentChange::Domain::StartupItem, state_.lastCheckSample)) {
    std::vector<StartupItemEvidence> currentStartup;
    collector.CollectStartupItems(currentStartup);

    std::set<std::wstring> currentStartupNames;
    for (const auto& s : currentStartup) currentStartupNames.insert(s.name);

    for (const auto& name : currentStartupNames) {
      if (state_.knownStartupNames.find(name) == state_.knownStartupNames.end()) {
        EnvironmentChange change;
        change.domain = EnvironmentChange::Domain::StartupItem;
        change.kind = EnvironmentChange::ChangeKind::Added;
        change.entityName = name;
        change.evidence = L"startup_added";
        change.significant = true;
        EmitChange(change);
        allChanges.push_back(std::move(change));
        ++state_.startupChangesDetected;
        ++state_.significantChangesDetected;
      }
    }

    for (const auto& name : state_.knownStartupNames) {
      if (currentStartupNames.find(name) == currentStartupNames.end()) {
        EnvironmentChange change;
        change.domain = EnvironmentChange::Domain::StartupItem;
        change.kind = EnvironmentChange::ChangeKind::Removed;
        change.entityName = name;
        change.evidence = L"startup_removed";
        change.significant = true;
        EmitChange(change);
        allChanges.push_back(std::move(change));
        ++state_.startupChangesDetected;
        ++state_.significantChangesDetected;
      }
    }

    state_.lastStartupCheckSample = state_.lastCheckSample;
  }

  if (ShouldCheckDomain(EnvironmentChange::Domain::Security, state_.lastCheckSample)) {
    SecurityStateEvidence currentSecurity;
    collector.CollectSecurityState(currentSecurity);

    if (baseline_.security.uacEnabled >= 0 && currentSecurity.uacEnabled >= 0 &&
        baseline_.security.uacEnabled != currentSecurity.uacEnabled) {
      EnvironmentChange change;
      change.domain = EnvironmentChange::Domain::Security;
      change.kind = EnvironmentChange::ChangeKind::StateChanged;
      change.entityName = L"UAC";
      change.oldValue = std::to_wstring(baseline_.security.uacEnabled);
      change.newValue = std::to_wstring(currentSecurity.uacEnabled);
      change.evidence = L"uac_changed";
      change.significant = true;
      EmitChange(change);
      allChanges.push_back(std::move(change));
      ++state_.securityChangesDetected;
      ++state_.significantChangesDetected;
    }

    if (baseline_.security.firewallEnabled >= 0 && currentSecurity.firewallEnabled >= 0 &&
        baseline_.security.firewallEnabled != currentSecurity.firewallEnabled) {
      EnvironmentChange change;
      change.domain = EnvironmentChange::Domain::Security;
      change.kind = EnvironmentChange::ChangeKind::StateChanged;
      change.entityName = L"Firewall";
      change.oldValue = std::to_wstring(baseline_.security.firewallEnabled);
      change.newValue = std::to_wstring(currentSecurity.firewallEnabled);
      change.evidence = L"firewall_changed";
      change.significant = true;
      EmitChange(change);
      allChanges.push_back(std::move(change));
      ++state_.securityChangesDetected;
      ++state_.significantChangesDetected;
    }

    if (baseline_.security.defenderRealTimeProtection >= 0 && currentSecurity.defenderRealTimeProtection >= 0 &&
        baseline_.security.defenderRealTimeProtection != currentSecurity.defenderRealTimeProtection) {
      EnvironmentChange change;
      change.domain = EnvironmentChange::Domain::Security;
      change.kind = EnvironmentChange::ChangeKind::StateChanged;
      change.entityName = L"DefenderRealTime";
      change.oldValue = std::to_wstring(baseline_.security.defenderRealTimeProtection);
      change.newValue = std::to_wstring(currentSecurity.defenderRealTimeProtection);
      change.evidence = L"defender_rt_changed";
      change.significant = true;
      EmitChange(change);
      allChanges.push_back(std::move(change));
      ++state_.securityChangesDetected;
      ++state_.significantChangesDetected;
    }

    state_.lastSecurityCheckSample = state_.lastCheckSample;
  }

  if (ShouldCheckDomain(EnvironmentChange::Domain::Network, state_.lastCheckSample)) {
    std::vector<NetworkInterfaceEvidence> currentInterfaces;
    collector.CollectNetworkInterfaces(currentInterfaces);

    for (const auto& iface : currentInterfaces) {
      bool found = false;
      for (const auto& baseIface : baseline_.networkInterfaces) {
        if (baseIface.name == iface.name) {
          found = true;
          if (baseIface.operStatus != iface.operStatus) {
            EnvironmentChange change;
            change.domain = EnvironmentChange::Domain::Network;
            change.kind = EnvironmentChange::ChangeKind::StateChanged;
            change.entityName = iface.name;
            change.oldValue = L"status=" + std::to_wstring(baseIface.operStatus);
            change.newValue = L"status=" + std::to_wstring(iface.operStatus);
            change.evidence = L"adapter_status_changed";
            change.significant = true;
            EmitChange(change);
            allChanges.push_back(std::move(change));
            ++state_.networkChangesDetected;
            ++state_.significantChangesDetected;
          }
          if (baseIface.ipv4Addresses != iface.ipv4Addresses) {
            EnvironmentChange change;
            change.domain = EnvironmentChange::Domain::Network;
            change.kind = EnvironmentChange::ChangeKind::StateChanged;
            change.entityName = iface.name;
            change.evidence = L"ip_addresses_changed";
            change.significant = true;
            EmitChange(change);
            allChanges.push_back(std::move(change));
            ++state_.networkChangesDetected;
            ++state_.significantChangesDetected;
          }
          break;
        }
      }
      if (!found) {
        EnvironmentChange change;
        change.domain = EnvironmentChange::Domain::Network;
        change.kind = EnvironmentChange::ChangeKind::Added;
        change.entityName = iface.name;
        change.entityPath = iface.description;
        change.evidence = L"adapter_added";
        change.significant = true;
        EmitChange(change);
        allChanges.push_back(std::move(change));
        ++state_.networkChangesDetected;
        ++state_.significantChangesDetected;
      }
    }

    state_.lastNetworkCheckSample = state_.lastCheckSample;
  }

  if (config_.enableModuleDeepScan &&
      ShouldCheckDomain(EnvironmentChange::Domain::Module, state_.lastCheckSample)) {
    std::vector<ModuleEvidence> currentModules;
    collector.GetModuleDetector().CollectAllProcessModules(currentModules);

    std::set<std::wstring> currentModulePaths;
    for (const auto& mod : currentModules) currentModulePaths.insert(mod.fullPath);

    for (const auto& mod : currentModules) {
      if (state_.knownModulePaths.find(mod.fullPath) == state_.knownModulePaths.end()) {
        EnvironmentChange change;
        change.domain = EnvironmentChange::Domain::Module;
        change.kind = EnvironmentChange::ChangeKind::Added;
        change.entityName = mod.name;
        change.entityPath = mod.fullPath;
        change.processName = mod.processName;
        change.pid = mod.pid;
        change.evidence = L"classification=" + std::wstring(ModuleClassificationName(mod.classification));
        if (!mod.sha256Hash.empty()) change.evidence += L" hash=" + mod.sha256Hash;
        if (!mod.signerName.empty()) change.evidence += L" signer=" + mod.signerName;
        change.significant = (mod.classification != ModuleClassification::Trusted &&
                              mod.classification != ModuleClassification::Known);
        EmitChange(change);
        allChanges.push_back(std::move(change));
        ++state_.moduleChangesDetected;
        if (change.significant) ++state_.significantChangesDetected;
      }
    }

    for (const auto& path : state_.knownModulePaths) {
      if (currentModulePaths.find(path) == currentModulePaths.end()) {
        EnvironmentChange change;
        change.domain = EnvironmentChange::Domain::Module;
        change.kind = EnvironmentChange::ChangeKind::Removed;
        change.entityPath = path;
        change.evidence = L"module_unloaded";
        change.significant = false;
        EmitChange(change);
        allChanges.push_back(std::move(change));
        ++state_.moduleChangesDetected;
      }
    }

    state_.lastModuleCheckSample = state_.lastCheckSample;
  }

  state_.totalChangesDetected = state_.moduleChangesDetected + state_.driverChangesDetected +
    state_.processChangesDetected + state_.serviceChangesDetected + state_.startupChangesDetected +
    state_.securityChangesDetected + state_.networkChangesDetected + state_.filesystemChangesDetected +
    state_.firmwareChangesDetected;

  return allChanges;
}

void ChangeDetector::EmitChange(const EnvironmentChange& change) {
  for (auto& callback : changeCallbacks_) {
    callback(change);
  }
}

void ChangeDetector::RecordModuleObservation(const std::wstring& modulePath, std::uint64_t timestampNs) {
  if (state_.knownModulePaths.find(modulePath) == state_.knownModulePaths.end()) {
    state_.knownModulePaths.insert(modulePath);
    state_.moduleFirstSeenNs[modulePath] = timestampNs;
  }
}

void ChangeDetector::RecordDriverObservation(const std::wstring& driverName, std::uint64_t timestampNs) {
  if (state_.knownDriverNames.find(driverName) == state_.knownDriverNames.end()) {
    state_.knownDriverNames.insert(driverName);
    state_.driverFirstSeenNs[driverName] = timestampNs;
  }
}

void ChangeDetector::RecordProcessObservation(int pid, const std::wstring& processName, std::uint64_t timestampNs) {
  if (state_.knownProcessPids.find(pid) == state_.knownProcessPids.end()) {
    state_.knownProcessPids.insert(pid);
    state_.processFirstSeenNs[pid] = timestampNs;
  }
}

void ChangeDetector::RecordServiceObservation(const std::wstring& serviceName, std::uint64_t timestampNs) {
  if (state_.knownServiceNames.find(serviceName) == state_.knownServiceNames.end()) {
    state_.knownServiceNames.insert(serviceName);
    state_.serviceFirstSeenNs[serviceName] = timestampNs;
  }
}

void ChangeDetector::RecordStartupObservation(const std::wstring& itemName, std::uint64_t timestampNs) {
  if (state_.knownStartupNames.find(itemName) == state_.knownStartupNames.end()) {
    state_.knownStartupNames.insert(itemName);
    state_.startupFirstSeenNs[itemName] = timestampNs;
  }
}

bool ChangeDetector::IsModuleFirstSeen(const std::wstring& modulePath) const {
  return state_.knownModulePaths.find(modulePath) == state_.knownModulePaths.end();
}

bool ChangeDetector::IsDriverFirstSeen(const std::wstring& driverName) const {
  return state_.knownDriverNames.find(driverName) == state_.knownDriverNames.end();
}

bool ChangeDetector::IsProcessFirstSeen(int pid) const {
  return state_.knownProcessPids.find(pid) == state_.knownProcessPids.end();
}

bool ChangeDetector::IsServiceFirstSeen(const std::wstring& serviceName) const {
  return state_.knownServiceNames.find(serviceName) == state_.knownServiceNames.end();
}

bool ChangeDetector::IsStartupFirstSeen(const std::wstring& itemName) const {
  return state_.knownStartupNames.find(itemName) == state_.knownStartupNames.end();
}

std::uint64_t ChangeDetector::GetModuleFirstSeenTime(const std::wstring& modulePath) const {
  auto it = state_.moduleFirstSeenNs.find(modulePath);
  return it != state_.moduleFirstSeenNs.end() ? it->second : 0;
}

std::uint64_t ChangeDetector::GetDriverFirstSeenTime(const std::wstring& driverName) const {
  auto it = state_.driverFirstSeenNs.find(driverName);
  return it != state_.driverFirstSeenNs.end() ? it->second : 0;
}

std::uint64_t ChangeDetector::GetProcessFirstSeenTime(int pid) const {
  auto it = state_.processFirstSeenNs.find(pid);
  return it != state_.processFirstSeenNs.end() ? it->second : 0;
}

} // namespace monix
