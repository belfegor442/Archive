#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "../snapshot/SystemSnapshot.hpp"
#include "../../events/SystemEvent.hpp"

namespace monix {

struct AnomalyResult {
  std::wstring metricName;
  std::wstring anomalyType;
  std::wstring description;
  double currentValue = 0.0;
  double baselineMean = 0.0;
  double baselineStddev = 0.0;
  double deviationScore = 0.0;
  EventSeverity severity = EventSeverity::Info;
  uint64_t timestampNs = 0;
  int processId = 0;
  std::wstring processName;
  std::wstring correlationId;
};

class AnomalyDetector {
public:
  struct BaselineEntry {
    double sum = 0.0;
    double sumSq = 0.0;
    int count = 0;
    double minVal = 1e30;
    double maxVal = -1e30;

    double Mean() const { return count > 0 ? sum / count : 0.0; }
    double Variance() const {
      if (count < 2) return 0.0;
      double m = Mean();
      return (sumSq / count) - (m * m);
    }
    double Stddev() const { return Variance() > 0 ? std::sqrt(Variance()) : 1.0; }
  };

  void RecordBaseline(const std::wstring& metric, double value) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto& b = baselines_[metric];
    b.sum += value;
    b.sumSq += value * value;
    b.count++;
    if (value < b.minVal) b.minVal = value;
    if (value > b.maxVal) b.maxVal = value;
    if (b.count > 1000) {
      double m = b.mean;
      double halfSum = m * 500;
      double halfSumSq = (b.variance + m * m) * 500;
      b.sum = halfSum + value;
      b.sumSq = halfSumSq + value * value;
      b.count = 501;
    }
  }

  void RecordSnapshot(const SystemSnapshot& snap) {
    std::lock_guard<std::mutex> lock(mutex_);
    snapshots_.push_back(snap);
    if (snapshots_.size() > 200) snapshots_.pop_front();
  }

  BaselineEntry GetBaseline(const std::wstring& metric) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = baselines_.find(metric);
    return (it != baselines_.end()) ? it->second : BaselineEntry{};
  }

  std::vector<AnomalyResult> Detect(const SystemSnapshot& current, uint64_t timestampNs) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<AnomalyResult> anomalies;

    DetectSimple(anomalies, timestampNs);
    DetectPattern(anomalies, timestampNs);
    DetectProcess(anomalies, timestampNs);

    return anomalies;
  }

  std::map<std::wstring, BaselineEntry> AllBaselines() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return baselines_;
  }

  void Clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    baselines_.clear();
    snapshots_.clear();
    processHistory_.clear();
  }

private:
  void DetectSimple(std::vector<AnomalyResult>& results, uint64_t tsNs) {
    if (snapshots_.empty()) return;
    const auto& snap = snapshots_.back();

    auto check = [&](const std::wstring& name, double value, double warn, double crit) {
      auto it = baselines_.find(name);
      if (it == baselines_.end()) return;
      const auto& b = it->second;
      if (b.count < 10) return;

      double mean = b.mean;
      double stddev = b.stddev;
      if (stddev < 0.001) stddev = 1.0;

      double deviation = (value - mean) / stddev;

      if (deviation > 3.0 || deviation < -3.0) {
        AnomalyResult r;
        r.metricName = name;
        r.anomalyType = (deviation > 3.0) ? L"spike" : L"drop";
        r.currentValue = value;
        r.baselineMean = mean;
        r.baselineStddev = stddev;
        r.deviationScore = deviation;
        r.timestampNs = tsNs;
        r.severity = (deviation > 4.0 || deviation < -4.0) ?
          EventSeverity::Critical : EventSeverity::High;
        r.correlationId = L"ANOM-" + name + L"-" + std::to_wstring(tsNs);
        r.description = name + L" " + r.anomalyType + L": " +
          std::to_wstring(static_cast<int>(deviation)) + "σ from mean";
        results.push_back(std::move(r));
      }
    };

    check(L"cpu.pct", snap.cpu.pct, 80.0, 95.0);
    check(L"memory.usedPct", snap.memory.UsedPct(), 85.0, 95.0);
    check(L"gpu.pct", snap.gpu.pct, 80.0, 95.0);
    check(L"gpu.tempC", snap.gpu.tempC, 80.0, 90.0);
    check(L"thermal.cpuCoreTempC", snap.thermal.cpuCoreTempC, 75.0, 85.0);
    check(L"thermal.cpuPackageTempC", snap.thermal.cpuPackageTempC, 80.0, 90.0);
    check(L"network.rttMs", snap.network.rttMs, 100.0, 200.0);
    check(L"network.downKbps", snap.network.downKbps, 5000.0, 10000.0);
    check(L"storage.diskReadMBs", snap.storage.diskReadMBs, 500.0, 1000.0);
    check(L"storage.diskWriteMBs", snap.storage.diskWriteMBs, 500.0, 1000.0);
    check(L"processes.count", static_cast<double>(snap.processes.count), 500.0, 1000.0);
  }

  void DetectPattern(std::vector<AnomalyResult>& results, uint64_t tsNs) {
    if (snapshots_.size() < 5) return;

    std::vector<double> cpuVals;
    for (auto it = snapshots_.rbegin();
         it != snapshots_.rend() && cpuVals.size() < 5; ++it) {
      cpuVals.push_back(it->cpu.pct);
    }

    bool monotonicRising = true;
    for (std::size_t i = 1; i < cpuVals.size(); ++i) {
      if (cpuVals[i] <= cpuVals[i - 1]) { monotonicRising = false; break; }
    }

    if (monotonicRising && cpuVals.size() >= 3) {
      AnomalyResult r;
      r.metricName = L"cpu.pct";
      r.anomalyType = L"pattern_rising";
      r.currentValue = cpuVals.front();
      r.deviationScore = cpuVals.front() - cpuVals.back();
      r.timestampNs = tsNs;
      r.severity = EventSeverity::Medium;
      r.correlationId = L"PATTERN-CPU-RISING-" + std::to_wstring(tsNs);
      r.description = L"CPU showing monotonic rise over 5 samples";
      results.push_back(std::move(r));
    }

    std::vector<double> tempVals;
    for (auto it = snapshots_.rbegin();
         it != snapshots_.rend() && tempVals.size() < 5; ++it) {
      tempVals.push_back(it->thermal.cpuCoreTempC);
    }

    bool tempRising = true;
    for (std::size_t i = 1; i < tempVals.size(); ++i) {
      if (tempVals[i] <= tempVals[i - 1]) { tempRising = false; break; }
    }

    if (tempRising && tempVals.size() >= 3 && tempVals.front() > 70.0) {
      AnomalyResult r;
      r.metricName = L"thermal.cpuCoreTempC";
      r.anomalyType = L"pattern_rising";
      r.currentValue = tempVals.front();
      r.timestampNs = tsNs;
      r.severity = EventSeverity::High;
      r.correlationId = L"PATTERN-TEMP-RISING-" + std::to_wstring(tsNs);
      r.description = L"CPU temperature rising with " +
        std::to_wstring(static_cast<int>(tempVals.front())) + L"C current";
      results.push_back(std::move(r));
    }
  }

  void DetectProcess(std::vector<AnomalyResult>& results, uint64_t tsNs) {
    if (snapshots_.empty()) return;
    const auto& snap = snapshots_.back();

    for (const auto& proc : snap.processes.list) {
      auto& history = processHistory_[proc.pid];
      history.cpuValues.push_back(proc.cpuPct);
      history.ramValues.push_back(proc.ramBytes);
      if (history.cpuValues.size() > 20) {
        history.cpuValues.pop_front();
        history.ramValues.pop_front();
      }

      if (history.cpuValues.size() >= 3) {
        double sum = 0;
        for (double v : history.cpuValues) sum += v;
        double avg = sum / history.cpuValues.size();
        if (proc.cpuPct > avg * 2.0 && proc.cpuPct > 30.0) {
          AnomalyResult r;
          r.metricName = L"process.cpu";
          r.anomalyType = L"process_burst";
          r.currentValue = proc.cpuPct;
          r.baselineMean = avg;
          r.deviationScore = proc.cpuPct / avg;
          r.timestampNs = tsNs;
          r.processId = proc.pid;
          r.processName = proc.name;
          r.severity = EventSeverity::Medium;
          r.correlationId = L"ANOM-PROC-" + std::to_wstring(proc.pid) +
            L"-" + std::to_wstring(tsNs);
          r.description = proc.name + L" (PID " + std::to_wstring(proc.pid) +
            L") CPU burst: " + std::to_wstring(static_cast<int>(proc.cpuPct)) +
            L"% vs avg " + std::to_wstring(static_cast<int>(avg)) + L"%";
          results.push_back(std::move(r));
        }
      }
    }
  }

  struct ProcessHistory {
    std::deque<double> cpuValues;
    std::deque<uint64_t> ramValues;
  };

  std::map<std::wstring, BaselineEntry> baselines_;
  std::deque<SystemSnapshot> snapshots_;
  std::map<int, ProcessHistory> processHistory_;
  mutable std::mutex mutex_;
};

}
