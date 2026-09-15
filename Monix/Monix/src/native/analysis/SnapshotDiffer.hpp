#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <algorithm>

#include "../core/snapshot/SystemSnapshot.hpp"
#include "../events/SystemEvent.hpp"

namespace monix {

enum class DiffType : uint8_t {
  ValueChanged, ItemAdded, ItemRemoved,
  StateChanged, ThresholdCrossed, ConfigChanged, SetChanged
};

struct DiffField {
  const char* path = nullptr;
  DiffType type = DiffType::ValueChanged;
  std::wstring previousValue;
  std::wstring currentValue;
  double delta = 0.0;
  double deltaPercent = 0.0;
  EventSeverity severity = EventSeverity::Info;
  bool isSignificant = false;
};

struct SnapshotDiff {
  uint64_t id = 0;
  uint64_t timestampNs = 0;
  uint64_t snapshotIdBefore = 0;
  uint64_t snapshotIdAfter = 0;
  std::vector<DiffField> fields;

  bool HasChanges() const { return !fields.empty(); }
  int ChangeCount() const { return static_cast<int>(fields.size()); }

  bool HasSignificantChanges() const {
    for (const auto& f : fields) {
      if (f.isSignificant) return true;
    }
    return false;
  }

  std::vector<DiffField> ChangesByPrefix(const char* prefix) const {
    std::vector<DiffField> result;
    for (const auto& f : fields) {
      if (std::string(f.path).find(prefix) == 0) {
        result.push_back(f);
      }
    }
    return result;
  }

  std::vector<DiffField> HighSeverityChanges() const {
    std::vector<DiffField> result;
    for (const auto& f : fields) {
      if (f.severity >= EventSeverity::High) {
        result.push_back(f);
      }
    }
    return result;
  }

  std::vector<DiffField> ThresholdCrossings() const {
    std::vector<DiffField> result;
    for (const auto& f : fields) {
      if (f.type == DiffType::ThresholdCrossed) {
        result.push_back(f);
      }
    }
    return result;
  }
};

class SnapshotDiffer {
public:
  SnapshotDiff Compute(const SystemSnapshot& a, const SystemSnapshot& b) {
    SnapshotDiff diff;
    diff.id = nextDiffId_++;
    diff.timestampNs = b.timestampNs;
    diff.snapshotIdBefore = a.id;
    diff.snapshotIdAfter = b.id;

    DiffCpu(a.cpu, b.cpu, diff);
    DiffMemory(a.memory, b.memory, diff);
    DiffGpu(a.gpu, b.gpu, diff);
    DiffStorage(a.storage, b.storage, diff);
    DiffNetwork(a.network, b.network, diff);
    DiffProcesses(a.processes, b.processes, diff);
    DiffPower(a.power, b.power, diff);
    DiffThermal(a.thermal, b.thermal, diff);
    DiffSecurity(a.security, b.security, diff);

    return diff;
  }

  void SetThreshold(const char* field, double warn, double critical) {
    thresholds_[field] = {warn, critical};
  }

  void AddIgnoredField(const char* field) {
    ignored_.insert(field);
  }

  void SetMinDeltaPercent(const char* field, double pct) {
    minDeltaPct_[field] = pct;
  }

private:
  bool IsIgnored(const char* path) const {
    return ignored_.count(path) > 0;
  }

  std::pair<double, double> GetThresholds(const char* path) const {
    auto it = thresholds_.find(path);
    if (it != thresholds_.end()) return it->second;
    return {0.0, 0.0};
  }

  double GetMinDeltaPct(const char* path) const {
    auto it = minDeltaPct_.find(path);
    if (it != minDeltaPct_.end()) return it->second;
    return 0.0;
  }

  EventSeverity ComputeSeverity(double absDelta, double pctDelta,
                                 double warnThreshold, double criticalThreshold) const {
    if (criticalThreshold > 0 && absDelta >= criticalThreshold) {
      return EventSeverity::Critical;
    }
    if (warnThreshold > 0 && absDelta >= warnThreshold) {
      return EventSeverity::High;
    }
    if (pctDelta > 50.0) return EventSeverity::High;
    if (pctDelta > 20.0) return EventSeverity::Medium;
    if (pctDelta > 5.0) return EventSeverity::Low;
    return EventSeverity::Info;
  }

  void DiffScalar(const std::string& path, double a, double b, SnapshotDiff& diff) {
    DiffScalar(path.c_str(), a, b, diff);
  }

  void DiffScalar(const char* path, double a, double b, SnapshotDiff& diff) {
    if (IsIgnored(path)) return;
    if (a == b) return;

    double delta = b - a;
    double absDelta = (delta < 0) ? -delta : delta;
    double pctDelta = (a != 0.0) ? (absDelta / (a < 0 ? -a : a)) * 100.0 : 0.0;

    double minPct = GetMinDeltaPct(path);
    if (minPct > 0 && pctDelta < minPct) return;

    auto [warn, crit] = GetThresholds(path);
    EventSeverity sev = ComputeSeverity(absDelta, pctDelta, warn, crit);

    DiffType diffType = (warn > 0 && b >= warn) ? DiffType::ThresholdCrossed
                                                  : DiffType::ValueChanged;
    bool significant = (sev >= EventSeverity::Medium) ||
                       (diffType == DiffType::ThresholdCrossed);

    DiffField field;
    field.path = path;
    field.type = diffType;
    field.delta = delta;
    field.deltaPercent = pctDelta;
    field.severity = sev;
    field.isSignificant = significant;
    field.previousValue = std::to_wstring(static_cast<long long>(a));
    field.currentValue = std::to_wstring(static_cast<long long>(b));
    diff.fields.push_back(field);
  }

  void DiffBool(const char* path, int a, int b, SnapshotDiff& diff) {
    if (IsIgnored(path)) return;
    if (a == b) return;

    DiffField field;
    field.path = path;
    field.type = DiffType::StateChanged;
    field.previousValue = a ? L"true" : L"false";
    field.currentValue = b ? L"true" : L"false";
    field.severity = EventSeverity::Medium;
    field.isSignificant = true;
    diff.fields.push_back(field);
  }

  void DiffCpu(const CpuState& a, const CpuState& b, SnapshotDiff& diff) {
    DiffScalar("cpu.pct", a.pct, b.pct, diff);
    DiffScalar("cpu.kernelTimePct", a.kernelTimePct, b.kernelTimePct, diff);
    DiffScalar("cpu.userTimePct", a.userTimePct, b.userTimePct, diff);
    DiffScalar("cpu.contextSwitchesPerSec", a.contextSwitchesPerSec,
               b.contextSwitchesPerSec, diff);
    DiffScalar("cpu.interruptsPerSec", a.interruptsPerSec,
               b.interruptsPerSec, diff);
    DiffScalar("cpu.processorQueueLength", a.processorQueueLength,
               b.processorQueueLength, diff);
    DiffScalar("cpu.threadCountDelta", a.threadCountDelta,
               b.threadCountDelta, diff);
  }

  void DiffMemory(const MemoryState& a, const MemoryState& b, SnapshotDiff& diff) {
    DiffScalar("memory.usedBytes", a.usedBytes, b.usedBytes, diff);
    DiffScalar("memory.availBytes", a.availBytes, b.availBytes, diff);
    DiffScalar("memory.commitUsedBytes", a.commitUsedBytes, b.commitUsedBytes, diff);
    DiffScalar("memory.pageFaultsDelta", a.pageFaultsDelta, b.pageFaultsDelta, diff);
    DiffScalar("memory.hardPageFaultsDelta", a.hardPageFaultsDelta,
               b.hardPageFaultsDelta, diff);
  }

  void DiffGpu(const GpuState& a, const GpuState& b, SnapshotDiff& diff) {
    if (a.pctValid && b.pctValid) {
      DiffScalar("gpu.pct", a.pct, b.pct, diff);
    }
    DiffScalar("gpu.tempC", a.tempC, b.tempC, diff);
    DiffScalar("gpu.vramUsedBytes", a.vramUsedBytes, b.vramUsedBytes, diff);
    DiffScalar("gpu.powerWatts", a.powerWatts, b.powerWatts, diff);
    DiffScalar("gpu.fanRpm", a.fanRpm, b.fanRpm, diff);
    DiffScalar("gpu.frameTimeMs", a.frameTimeMs, b.frameTimeMs, diff);
    if (a.model != b.model) {
      DiffField field;
      field.path = "gpu.model";
      field.type = DiffType::ConfigChanged;
      field.previousValue = a.model;
      field.currentValue = b.model;
      field.severity = EventSeverity::Medium;
      field.isSignificant = true;
      diff.fields.push_back(field);
    }
  }

  void DiffStorage(const StorageState& a, const StorageState& b, SnapshotDiff& diff) {
    DiffScalar("storage.readBytesPerSec", a.readBytesPerSec, b.readBytesPerSec, diff);
    DiffScalar("storage.writeBytesPerSec", a.writeBytesPerSec, b.writeBytesPerSec, diff);
    DiffScalar("storage.queueLength", a.queueLength, b.queueLength, diff);
    DiffScalar("storage.readLatencyMs", a.readLatencyMs, b.readLatencyMs, diff);
    DiffScalar("storage.writeLatencyMs", a.writeLatencyMs, b.writeLatencyMs, diff);
    DiffScalar("storage.nvmeTempC", a.nvmeTempC, b.nvmeTempC, diff);
    DiffScalar("storage.readIops", a.readIops, b.readIops, diff);
    DiffScalar("storage.writeIops", a.writeIops, b.writeIops, diff);
    if (a.smartHealthOk != b.smartHealthOk) {
      DiffField field;
      field.path = "storage.smartHealthOk";
      field.type = DiffType::StateChanged;
      field.previousValue = std::to_wstring(a.smartHealthOk);
      field.currentValue = std::to_wstring(b.smartHealthOk);
      field.severity = (b.smartHealthOk == 0) ? EventSeverity::Critical
                                               : EventSeverity::Info;
      field.isSignificant = true;
      diff.fields.push_back(field);
    }
  }

  void DiffNetwork(const NetworkState& a, const NetworkState& b, SnapshotDiff& diff) {
    DiffScalar("network.upBytesPerSec", a.upBytesPerSec, b.upBytesPerSec, diff);
    DiffScalar("network.downBytesPerSec", a.downBytesPerSec, b.downBytesPerSec, diff);
    DiffScalar("network.inboundConnections", a.inboundConnections,
               b.inboundConnections, diff);
    DiffScalar("network.outboundConnections", a.outboundConnections,
               b.outboundConnections, diff);
    DiffScalar("network.latencyMs", a.latencyMs, b.latencyMs, diff);
    DiffScalar("network.dnsResolutionMs", a.dnsResolutionMs, b.dnsResolutionMs, diff);
    DiffScalar("network.tcpRetransmits", a.tcpRetransmits, b.tcpRetransmits, diff);
    DiffScalar("network.tcpResets", a.tcpResets, b.tcpResets, diff);
    if (a.dnsResolutionOk != b.dnsResolutionOk) {
      DiffField field;
      field.path = "network.dnsResolutionOk";
      field.type = DiffType::StateChanged;
      field.previousValue = std::to_wstring(a.dnsResolutionOk);
      field.currentValue = std::to_wstring(b.dnsResolutionOk);
      field.severity = (b.dnsResolutionOk == 0) ? EventSeverity::High
                                                  : EventSeverity::Info;
      field.isSignificant = true;
      diff.fields.push_back(field);
    }
    if (a.adapterAddresses != b.adapterAddresses) {
      DiffField field;
      field.path = "network.adapters";
      field.type = DiffType::SetChanged;
      field.severity = EventSeverity::Medium;
      field.isSignificant = true;
      field.previousValue = std::to_wstring(a.adapterAddresses.size()) + L" adapters";
      field.currentValue = std::to_wstring(b.adapterAddresses.size()) + L" adapters";
      diff.fields.push_back(field);
    }
  }

  void DiffProcesses(const ProcessTable& a, const ProcessTable& b, SnapshotDiff& diff) {
    DiffScalar("processes.count", a.count, b.count, diff);
    DiffScalar("processes.threadCount", a.threadCount, b.threadCount, diff);
    DiffScalar("processes.handleCount", a.handleCount, b.handleCount, diff);

    std::unordered_map<int, const ProcessInfo*> aMap;
    for (const auto& p : a.list) aMap[p.pid] = &p;

    for (const auto& bp : b.list) {
      auto it = aMap.find(bp.pid);
      if (it == aMap.end()) {
        DiffField field;
        field.path = "processes.started";
        field.type = DiffType::ItemAdded;
        field.currentValue = bp.name + L" (PID " + std::to_wstring(bp.pid) + L")";
        field.severity = EventSeverity::Info;
        field.isSignificant = true;
        diff.fields.push_back(field);
      } else {
        const auto* ap = it->second;
        if (ap->cpuPct != bp.cpuPct) {
          DiffScalar("processes.cpu." + std::to_string(bp.pid),
                     ap->cpuPct, bp.cpuPct, diff);
        }
        if (ap->ramBytes != bp.ramBytes) {
          DiffScalar("processes.ram." + std::to_string(bp.pid),
                     static_cast<double>(ap->ramBytes),
                     static_cast<double>(bp.ramBytes), diff);
        }
      }
    }

    std::unordered_map<int, bool> bPids;
    for (const auto& p : b.list) bPids[p.pid] = true;
    for (const auto& ap : a.list) {
      if (bPids.find(ap.pid) == bPids.end()) {
        DiffField field;
        field.path = "processes.stopped";
        field.type = DiffType::ItemRemoved;
        field.previousValue = ap.name + L" (PID " + std::to_wstring(ap.pid) + L")";
        field.severity = EventSeverity::Info;
        field.isSignificant = true;
        diff.fields.push_back(field);
      }
    }
  }

  void DiffPower(const PowerState& a, const PowerState& b, SnapshotDiff& diff) {
    if (a.acLineStatus != b.acLineStatus) {
      DiffField field;
      field.path = "power.acLineStatus";
      field.type = DiffType::StateChanged;
      field.previousValue = std::to_wstring(a.acLineStatus);
      field.currentValue = std::to_wstring(b.acLineStatus);
      field.severity = EventSeverity::Medium;
      field.isSignificant = true;
      diff.fields.push_back(field);
    }
    DiffScalar("power.batteryLifePercent", a.batteryLifePercent, b.batteryLifePercent, diff);
    DiffScalar("power.batteryTemperature", a.batteryTemperature, b.batteryTemperature, diff);
    DiffScalar("power.batteryWearLevel", a.batteryWearLevel, b.batteryWearLevel, diff);
    if (a.powerPlanIndex != b.powerPlanIndex) {
      DiffField field;
      field.path = "power.plan";
      field.type = DiffType::ConfigChanged;
      field.previousValue = std::to_wstring(a.powerPlanIndex);
      field.currentValue = std::to_wstring(b.powerPlanIndex);
      field.severity = EventSeverity::Low;
      field.isSignificant = true;
      diff.fields.push_back(field);
    }
  }

  void DiffThermal(const ThermalState& a, const ThermalState& b, SnapshotDiff& diff) {
    DiffScalar("thermal.cpuCoreTempC", a.cpuCoreTempC, b.cpuCoreTempC, diff);
    DiffScalar("thermal.gpuTempC", 0, 0, diff); // handled in DiffGpu
    DiffScalar("thermal.motherboardTempC", a.motherboardTempC, b.motherboardTempC, diff);
    DiffScalar("thermal.vrmTempC", a.vrmTempC, b.vrmTempC, diff);
    DiffBool("thermal.cpuThrottling", a.cpuThrottling, b.cpuThrottling, diff);
    DiffScalar("thermal.fanCount", a.fanCount, b.fanCount, diff);
    DiffScalar("thermal.pumpSpeed", a.pumpSpeed, b.pumpSpeed, diff);
    DiffScalar("thermal.thermalSensorFailures", a.thermalSensorFailures,
               b.thermalSensorFailures, diff);
    DiffScalar("thermal.thermalRecoveryCount", a.thermalRecoveryCount,
               b.thermalRecoveryCount, diff);
  }

  void DiffSecurity(const SecurityState& a, const SecurityState& b, SnapshotDiff& diff) {
    DiffScalar("security.unsignedDriverCount", a.unsignedDriverCount,
               b.unsignedDriverCount, diff);
    DiffScalar("security.suspiciousScriptHosts", a.suspiciousScriptHosts,
               b.suspiciousScriptHosts, diff);
    DiffScalar("security.uacConsentProcesses", a.uacConsentProcesses,
               b.uacConsentProcesses, diff);
    DiffScalar("security.lsassAccessCount", a.lsassAccessCount,
               b.lsassAccessCount, diff);
    DiffBool("security.debugPortActive", a.debugPortActive, b.debugPortActive, diff);
    DiffScalar("security.hookModulesDetected", a.hookModulesDetected,
               b.hookModulesDetected, diff);
    DiffScalar("security.peHeaderTamper", a.peHeaderTamper, b.peHeaderTamper, diff);
    if (a.unsignedDriverNames != b.unsignedDriverNames) {
      DiffField field;
      field.path = "security.unsignedDriverNames";
      field.type = DiffType::SetChanged;
      field.severity = EventSeverity::High;
      field.isSignificant = true;
      diff.fields.push_back(field);
    }
  }

  std::unordered_map<std::string, std::pair<double, double>> thresholds_;
  std::unordered_set<std::string> ignored_;
  std::unordered_map<std::string, double> minDeltaPct_;
  uint64_t nextDiffId_ = 1;
};

}
