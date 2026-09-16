#pragma once

#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "../../events/SystemEvent.hpp"
#include "../snapshot/SystemSnapshot.hpp"

namespace monix {

struct CorrelatedSignal {
  uint64_t timestampNs = 0;
  std::wstring source;
  std::wstring signalType;
  double value = 0.0;
  std::wstring detail;
  EventCategory category = EventCategory::System;
  EventSeverity severity = EventSeverity::Info;
  int processId = 0;
  std::wstring processName;
};

struct CorrelationCluster {
  std::wstring clusterId;
  std::wstring pattern;
  std::wstring rootCause;
  std::wstring summary;
  std::vector<CorrelatedSignal> signals;
  int severity = 0;
  double confidence = 0.0;
  uint64_t startNs = 0;
  uint64_t endNs = 0;
  std::wstring correlationId;
};

class CorrelationEngine {
public:
  void AddSignal(const CorrelatedSignal& signal) {
    std::lock_guard<std::mutex> lock(mutex_);
    signals_.push_back(signal);
    if (signals_.size() > 500) {
      signals_.erase(signals_.begin());
    }
  }

  void SetWindowNs(uint64_t windowNs) { windowNs_ = windowNs; }

  std::vector<CorrelatedSignal> RecentSignals(int count = 50) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<CorrelatedSignal> result;
    int n = std::min(count, static_cast<int>(signals_.size()));
    for (int i = static_cast<int>(signals_.size()) - n;
         i < static_cast<int>(signals_.size()); ++i) {
      result.push_back(signals_[i]);
    }
    return result;
  }

  std::vector<CorrelatedSignal> SignalsByProcess(int pid) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<CorrelatedSignal> result;
    for (const auto& s : signals_) {
      if (s.processId == pid) result.push_back(s);
    }
    return result;
  }

  std::vector<CorrelatedSignal> SignalsByCategory(EventCategory cat) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<CorrelatedSignal> result;
    for (const auto& s : signals_) {
      if (s.category == cat) result.push_back(s);
    }
    return result;
  }

  std::vector<CorrelatedSignal> SignalsByType(const std::wstring& type) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<CorrelatedSignal> result;
    for (const auto& s : signals_) {
      if (s.signalType == type) result.push_back(s);
    }
    return result;
  }

  std::vector<CorrelatedSignal> SignalsInRange(uint64_t startNs, uint64_t endNs) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<CorrelatedSignal> result;
    for (const auto& s : signals_) {
      if (s.timestampNs >= startNs && s.timestampNs <= endNs) {
        result.push_back(s);
      }
    }
    return result;
  }

  std::map<std::wstring, int> SignalTypeCounts() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::map<std::wstring, int> counts;
    for (const auto& s : signals_) {
      counts[s.signalType]++;
    }
    return counts;
  }

  CorrelationCluster FindCluster(uint64_t startNs, uint64_t endNs) {
    std::lock_guard<std::mutex> lock(mutex_);
    CorrelationCluster cluster;
    cluster.startNs = startNs;
    cluster.endNs = endNs;

    for (const auto& s : signals_) {
      if (s.timestampNs >= startNs && s.timestampNs <= endNs) {
        cluster.signals.push_back(s);
      }
    }

    if (cluster.signals.empty()) return cluster;

    AnalyzeCluster(cluster);
    return cluster;
  }

  std::vector<CorrelationCluster> DetectClusters(uint64_t nowNs) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<CorrelationCluster> clusters;

    if (signals_.empty()) return clusters;

    uint64_t startNs = signals_.front().timestampNs;
    uint64_t endNs = nowNs;

    for (uint64_t t = startNs; t + windowNs_ <= endNs; t += windowNs_ / 2) {
      CorrelationCluster cluster;
      cluster.startNs = t;
      cluster.endNs = t + windowNs_;

      for (const auto& s : signals_) {
        if (s.timestampNs >= cluster.startNs && s.timestampNs <= cluster.endNs) {
          cluster.signals.push_back(s);
        }
      }

      if (cluster.signals.size() >= 2) {
        AnalyzeCluster(cluster);
        if (cluster.confidence > 0.3) {
          clusters.push_back(std::move(cluster));
        }
      }
    }

    return clusters;
  }

  void Clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    signals_.clear();
  }

  int SignalCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return static_cast<int>(signals_.size());
  }

private:
  void AnalyzeCluster(CorrelationCluster& cluster) {
    if (cluster.signals.empty()) return;

    cluster.correlationId = L"CORR-" + std::to_wstring(cluster.startNs);

    int cpuSignals = 0, gpuSignals = 0, netSignals = 0, thermalSignals = 0;
    int processSignals = 0, storageSignals = 0, securitySignals = 0;
    double maxSeverity = 0;
    std::map<int, int> processCounts;

    for (const auto& s : cluster.signals) {
      if (s.category == EventCategory::Hardware) cpuSignals++;
      if (s.category == EventCategory::Hardware) gpuSignals++;
      if (s.category == EventCategory::Network) netSignals++;
      if (s.category == EventCategory::Thermal) thermalSignals++;
      if (s.category == EventCategory::Process) processSignals++;
      if (s.category == EventCategory::Storage) storageSignals++;
      if (s.category == EventCategory::Security) securitySignals++;
      if (s.severity > maxSeverity) maxSeverity = s.severity;
      if (s.processId > 0) processCounts[s.processId]++;
    }

    cluster.severity = static_cast<int>(maxSeverity);

    int totalSignals = static_cast<int>(cluster.signals.size());
    int uniqueProcesses = static_cast<int>(processCounts.size());

    if (cpuSignals > 0 && netSignals > 0 && thermalSignals > 0) {
      cluster.pattern = L"triple_correlation";
      cluster.rootCause = L"CPU + network + thermal correlated — possible stress test or mining";
      cluster.confidence = 0.9;
    } else if (cpuSignals > 0 && processSignals > 0 && uniqueProcesses == 1) {
      cluster.pattern = L"single_process_burst";
      cluster.rootCause = L"Single process causing CPU burst";
      cluster.confidence = 0.8;
    } else if (cpuSignals > 0 && netSignals > 0) {
      cluster.pattern = L"cpu_network_correlation";
      cluster.rootCause = L"CPU and network spike correlated — unusual workload";
      cluster.confidence = 0.7;
    } else if (cpuSignals > 0 && thermalSignals > 0) {
      cluster.pattern = L"thermal_throttle_risk";
      cluster.rootCause = L"CPU spike with elevated temperature — throttle risk";
      cluster.confidence = 0.7;
    } else if (netSignals > 0 && securitySignals > 0) {
      cluster.pattern = L"network_security";
      cluster.rootCause = L"Network activity with security event — possible breach";
      cluster.confidence = 0.8;
    } else if (storageSignals > 0 && cpuSignals > 0) {
      cluster.pattern = L"disk_cpu_bottleneck";
      cluster.rootCause = L"Disk I/O correlated with CPU — possible disk bottleneck";
      cluster.confidence = 0.6;
    } else if (totalSignals >= 3) {
      cluster.pattern = L"multi_signal_correlation";
      cluster.rootCause = L"Multiple signals correlated";
      cluster.confidence = 0.5;
    } else {
      cluster.pattern = L"single_signal";
      cluster.confidence = 0.2;
    }

    cluster.summary = L"Pattern: " + cluster.pattern + L" | Signals: " +
      std::to_wstring(totalSignals) + L" | Confidence: " +
      std::to_wstring(static_cast<int>(cluster.confidence * 100)) + L"%";
  }

  std::vector<CorrelatedSignal> signals_;
  uint64_t windowNs_ = 5000000000ULL;
  mutable std::mutex mutex_;
};

}
