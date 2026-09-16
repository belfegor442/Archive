#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "../snapshot/SystemSnapshot.hpp"
#include "../../events/SystemEvent.hpp"
#include "../correlation/CorrelationEngine.hpp"
#include "../anomaly/AnomalyDetector.hpp"

namespace monix {

struct ScramRule2 {
  std::wstring id;
  std::wstring name;
  std::wstring description;
  std::wstring category;
  int defaultRiskScore = 0;
  int cooldownMinutes = 5;
  int debounceRequired = 2;

  std::function<bool(const SystemSnapshot&, const SystemSnapshot*, const CorrelationCluster*)> evaluate;
  std::function<std::wstring(const SystemSnapshot&, const SystemSnapshot*)> message;
};

struct ScramFinding2 {
  uint64_t id = 0;
  uint64_t timestampNs = 0;
  std::wstring ruleId;
  std::wstring headline;
  std::wstring insight;
  std::wstring context;
  int riskScore = 0;
  int cooldownMinutes = 0;
  uint64_t cooldownUntilNs = 0;
  bool suppressed = false;
  std::wstring correlationId;
  std::wstring processName;
  int processId = 0;
  int sessionFindingIndex = 0;
};

struct ScramFindingState2 {
  int debounceCount = 0;
  int cooldownRemaining = 0;
  bool wasEmitted = false;
  bool presentInLastTick = false;
  uint64_t lastEmittedNs = 0;
};

class ScramEngine2 {
public:
  ScramEngine2() { RegisterBuiltinRules(); }

  void AddRule(std::unique_ptr<ScramRule2> rule) {
    std::lock_guard<std::mutex> lock(mutex_);
    rules_.push_back(std::move(rule));
  }

  std::size_t RuleCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return rules_.size();
  }

  std::vector<ScramFinding2> Evaluate(const SystemSnapshot& current,
                                       const SystemSnapshot* previous,
                                       const CorrelationCluster* cluster,
                                       uint64_t timestampNs) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<ScramFinding2> findings;

    if (!calibrated_) {
      calibrationSamples_++;
      if (calibrationSamples_ >= 5) calibrated_ = true;
      return findings;
    }

    for (auto& rule : rules_) {
      bool present = rule->evaluate(current, previous, cluster);
      auto& state = findingState_[rule->id];

      if (present) {
        state.debounceCount++;
        if (state.debounceCount >= rule->debounceRequired) {
          if (!state.wasEmitted &&
              (timestampNs >= state.cooldownUntilNs)) {
            ScramFinding2 f;
            f.id = nextFindingId_++;
            f.timestampNs = timestampNs;
            f.ruleId = rule->id;
            f.headline = rule->name;
            f.insight = rule->message(current, previous);
            f.riskScore = rule->defaultRiskScore;
            f.cooldownMinutes = rule->cooldownMinutes;
            f.cooldownUntilNs = timestampNs +
              static_cast<uint64_t>(rule->cooldownMinutes) * 60000000000ULL;
            f.correlationId = L"SCRAM-" + std::to_wstring(f.id);
            f.processId = current.processes.topCpuPid;
            f.processName = current.processes.topCpuName;

            state.wasEmitted = true;
            state.lastEmittedNs = timestampNs;
            findings.push_back(std::move(f));
          }
        }
      } else {
        state.debounceCount = 0;
        state.wasEmitted = false;
      }
      state.presentInLastTick = present;
    }

    return findings;
  }

  void ClearState() {
    std::lock_guard<std::mutex> lock(mutex_);
    findingState_.clear();
  }

  void ResetCalibration() {
    std::lock_guard<std::mutex> lock(mutex_);
    calibrated_ = false;
    calibrationSamples_ = 0;
  }

  bool IsCalibrated() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return calibrated_;
  }

  const std::vector<std::unique_ptr<ScramRule2>>& Rules() const {
    return rules_;
  }

private:
  void RegisterBuiltinRules() {
    {
      auto rule = std::make_unique<ScramRule2>();
      rule->id = L"CPU_OVERLOAD";
      rule->name = L"CPU Overload";
      rule->description = L"CPU usage exceeds 90%";
      rule->category = L"hardware";
      rule->defaultRiskScore = 60;
      rule->cooldownMinutes = 5;
      rule->debounceRequired = 2;
      rule->evaluate = [](const SystemSnapshot& cur, const SystemSnapshot* prev, const CorrelationCluster*) {
        return cur.cpu.pct > 90.0;
      };
      rule->message = [](const SystemSnapshot& cur, const SystemSnapshot*) {
        return L"CPU at " + std::to_wstring(static_cast<int>(cur.cpu.pct)) +
          L"% — high utilization detected";
      };
      rules_.push_back(std::move(rule));
    }
    {
      auto rule = std::make_unique<ScramRule2>();
      rule->id = L"THERMAL_THROTTLE";
      rule->name = L"Thermal Throttle Risk";
      rule->description = L"CPU temperature exceeds 85C";
      rule->category = L"thermal";
      rule->defaultRiskScore = 75;
      rule->cooldownMinutes = 10;
      rule->debounceRequired = 3;
      rule->evaluate = [](const SystemSnapshot& cur, const SystemSnapshot*, const CorrelationCluster*) {
        return cur.thermal.cpuCoreTempC > 85.0;
      };
      rule->message = [](const SystemSnapshot& cur, const SystemSnapshot*) {
        return L"CPU at " + std::to_wstring(static_cast<int>(cur.thermal.cpuCoreTempC)) +
          L"C — throttle imminent";
      };
      rules_.push_back(std::move(rule));
    }
    {
      auto rule = std::make_unique<ScramRule2>();
      rule->id = L"MEMORY_PRESSURE";
      rule->name = L"Memory Pressure";
      rule->description = L"RAM usage exceeds 90%";
      rule->category = L"memory";
      rule->defaultRiskScore = 50;
      rule->cooldownMinutes = 5;
      rule->debounceRequired = 2;
      rule->evaluate = [](const SystemSnapshot& cur, const SystemSnapshot*, const CorrelationCluster*) {
        if (cur.memory.totalBytes == 0) return false;
        double usedPct = (static_cast<double>(cur.memory.usedBytes) /
          static_cast<double>(cur.memory.totalBytes)) * 100.0;
        return usedPct > 90.0;
      };
      rule->message = [](const SystemSnapshot& cur, const SystemSnapshot*) {
        if (cur.memory.totalBytes == 0) return std::wstring(L"Memory status unknown");
        double usedPct = (static_cast<double>(cur.memory.usedBytes) /
          static_cast<double>(cur.memory.totalBytes)) * 100.0;
        return L"RAM at " + std::to_wstring(static_cast<int>(usedPct)) +
          L"% — memory pressure";
      };
      rules_.push_back(std::move(rule));
    }
    {
      auto rule = std::make_unique<ScramRule2>();
      rule->id = L"GPU_OVERLOAD";
      rule->name = L"GPU Overload";
      rule->description = L"GPU usage exceeds 95%";
      rule->category = L"gpu";
      rule->defaultRiskScore = 55;
      rule->cooldownMinutes = 5;
      rule->debounceRequired = 2;
      rule->evaluate = [](const SystemSnapshot& cur, const SystemSnapshot*, const CorrelationCluster*) {
        return cur.gpu.pctValid && cur.gpu.pct > 95.0;
      };
      rule->message = [](const SystemSnapshot& cur, const SystemSnapshot*) {
        return L"GPU at " + std::to_wstring(static_cast<int>(cur.gpu.pct)) + L"%";
      };
      rules_.push_back(std::move(rule));
    }
    {
      auto rule = std::make_unique<ScramRule2>();
      rule->id = L"NETWORK_LATENCY";
      rule->name = L"High Network Latency";
      rule->description = L"RTT exceeds 200ms";
      rule->category = L"network";
      rule->defaultRiskScore = 40;
      rule->cooldownMinutes = 5;
      rule->debounceRequired = 3;
      rule->evaluate = [](const SystemSnapshot& cur, const SystemSnapshot*, const CorrelationCluster*) {
        return cur.network.pingRttMs > 200;
      };
      rule->message = [](const SystemSnapshot& cur, const SystemSnapshot*) {
        int rtt = (cur.network.pingRttMs >= 0) ? cur.network.pingRttMs : cur.network.latencyMs;
        return L"RTT at " + std::to_wstring(rtt) + L"ms";
      };
      rules_.push_back(std::move(rule));
    }
    {
      auto rule = std::make_unique<ScramRule2>();
      rule->id = L"DISK_FULL";
      rule->name = L"Disk Nearly Full";
      rule->description = L"Disk free space below 10%";
      rule->category = L"storage";
      rule->defaultRiskScore = 65;
      rule->cooldownMinutes = 15;
      rule->debounceRequired = 1;
      rule->evaluate = [](const SystemSnapshot& cur, const SystemSnapshot*, const CorrelationCluster*) {
        if (cur.storage.totalBytes == 0) return false;
        double freePct = (static_cast<double>(cur.storage.freeBytes) /
          static_cast<double>(cur.storage.totalBytes)) * 100.0;
        return freePct < 10.0;
      };
      rule->message = [](const SystemSnapshot& cur, const SystemSnapshot*) {
        if (cur.storage.totalBytes == 0) return std::wstring(L"Disk status unknown");
        double freePct = (static_cast<double>(cur.storage.freeBytes) /
          static_cast<double>(cur.storage.totalBytes)) * 100.0;
        return L"Disk free: " + std::to_wstring(static_cast<int>(freePct)) + L"%";
      };
      rules_.push_back(std::move(rule));
    }
    {
      auto rule = std::make_unique<ScramRule2>();
      rule->id = L"SECURITY_THREAT";
      rule->name = L"Security Threat Detected";
      rule->description = L"Defender threats detected";
      rule->category = L"security";
      rule->defaultRiskScore = 90;
      rule->cooldownMinutes = 30;
      rule->debounceRequired = 1;
      rule->evaluate = [](const SystemSnapshot& cur, const SystemSnapshot*, const CorrelationCluster*) {
        return cur.security.unsignedDriverCount > 0 ||
               cur.security.suspiciousScriptHosts > 0 ||
               cur.security.lsassAccessCount > 0;
      };
      rule->message = [](const SystemSnapshot& cur, const SystemSnapshot*) {
        int threats = cur.security.unsignedDriverCount + cur.security.suspiciousScriptHosts +
                      cur.security.lsassAccessCount;
        return L"Security issues detected: " + std::to_wstring(threats) + L" suspicious indicators";
      };
      rules_.push_back(std::move(rule));
    }
    {
      auto rule = std::make_unique<ScramRule2>();
      rule->id = L"RELIABILITY_CRASH";
      rule->name = L"System Crash Detected";
      rule->description = L"Recent crash dump found";
      rule->category = L"reliability";
      rule->defaultRiskScore = 85;
      rule->cooldownMinutes = 60;
      rule->debounceRequired = 1;
      rule->evaluate = [](const SystemSnapshot& cur, const SystemSnapshot*, const CorrelationCluster*) {
        return cur.reliability.crashEventsToday > 0 ||
               cur.reliability.unhandledExceptionCount > 0;
      };
      rule->message = [](const SystemSnapshot& cur, const SystemSnapshot*) {
        return L"Crash events today: " + std::to_wstring(cur.reliability.crashEventsToday) +
          L" | Exceptions: " + std::to_wstring(cur.reliability.unhandledExceptionCount);
      };
      rules_.push_back(std::move(rule));
    }
  }

  std::vector<std::unique_ptr<ScramRule2>> rules_;
  std::unordered_map<std::wstring, ScramFindingState2> findingState_;
  uint64_t nextFindingId_ = 1;
  bool calibrated_ = false;
  int calibrationSamples_ = 0;
  mutable std::mutex mutex_;
};

}
