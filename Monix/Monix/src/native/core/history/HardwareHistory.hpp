#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "../snapshot/SystemSnapshot.hpp"

namespace monix {

struct CpuHistorySample {
  uint64_t timestampNs = 0;
  double overallPct = 0.0;
  double perCore[64] = {};
  double packagePowerW = 0.0;
  int coresActive = 0;
};

struct GpuHistorySample {
  uint64_t timestampNs = 0;
  double pct = 0.0;
  double tempC = 0.0;
  double powerW = 0.0;
  uint64_t usedBytes = 0;
  uint64_t totalBytes = 0;
};

struct ThermalHistorySample {
  uint64_t timestampNs = 0;
  double cpuCoreTempC = 0.0;
  double cpuPackageTempC = 0.0;
  double gpuTempC = 0.0;
  double ssdTempC = 0.0;
  double fanSpeeds[10] = {};
  int fanCount = 0;
  int cpuThrottling = 0;
  uint32_t thermalLimitReason = 0;
};

struct StorageHistorySample {
  uint64_t timestampNs = 0;
  double diskReadMBs = 0.0;
  double diskWriteMBs = 0.0;
  double iops = 0.0;
  double queueDepth = 0.0;
  double activeTimePct = 0.0;
  int ready = 0;
  double wearPct = 0.0;
  double tempC = 0.0;
};

struct PowerHistorySample {
  uint64_t timestampNs = 0;
  int source = 0;
  int batteryPresent = 0;
  double batteryChargePct = 0.0;
  int batteryChargeCycles = 0;
  double batteryCapacityMWh = 0.0;
  double batteryWearPct = 0.0;
  int acConnected = 0;
  int acOnline = 0;
  double systemLoadW = 0.0;
  double totalPowerW = 0.0;
};

struct HardwareTrend {
  std::wstring metric;
  std::wstring direction;
  double slope = 0.0;
  double currentValue = 0.0;
  double predictedValue = 0.0;
  double confidence = 0.0;
  std::wstring description;
};

struct HardwareAlert {
  uint64_t timestampNs = 0;
  std::wstring component;
  std::wstring type;
  std::wstring description;
  double value = 0.0;
  double threshold = 0.0;
  EventSeverity severity = EventSeverity::Info;
};

class HardwareHistory {
public:
  void RecordSnapshot(const SystemSnapshot& snap) {
    std::lock_guard<std::mutex> lock(mutex_);

    CpuHistorySample cpu;
    cpu.timestampNs = snap.timestampNs;
    cpu.overallPct = snap.cpu.pct;
    memcpy(cpu.perCore, snap.cpu.cores, sizeof(cpu.perCore));
    cpu.packagePowerW = snap.cpu.packagePowerW;
    cpu.coresActive = snap.cpu.coresActive;
    cpuSamples_.push_back(cpu);
    if (cpuSamples_.size() > 500) cpuSamples_.pop_front();

    GpuHistorySample gpu;
    gpu.timestampNs = snap.timestampNs;
    gpu.pct = snap.gpu.pct;
    gpu.tempC = snap.gpu.tempC;
    gpu.powerW = snap.gpu.powerW;
    gpu.usedBytes = snap.gpu.usedBytes;
    gpu.totalBytes = snap.gpu.totalBytes;
    gpuSamples_.push_back(gpu);
    if (gpuSamples_.size() > 500) gpuSamples_.pop_front();

    ThermalHistorySample thermal;
    thermal.timestampNs = snap.timestampNs;
    thermal.cpuCoreTempC = snap.thermal.cpuCoreTempC;
    thermal.cpuPackageTempC = snap.thermal.cpuPackageTempC;
    thermal.gpuTempC = snap.thermal.gpuTempC;
    thermal.ssdTempC = snap.thermal.ssdTempC;
    memcpy(thermal.fanSpeeds, snap.thermal.fanSpeeds.data(), sizeof(thermal.fanSpeeds));
    thermal.fanCount = snap.thermal.fanCount;
    thermal.cpuThrottling = snap.thermal.cpuThrottling;
    thermal.thermalLimitReason = snap.thermal.thermalLimitReason;
    thermalSamples_.push_back(thermal);
    if (thermalSamples_.size() > 500) thermalSamples_.pop_front();

    StorageHistorySample storage;
    storage.timestampNs = snap.timestampNs;
    storage.diskReadMBs = snap.storage.diskReadMBs;
    storage.diskWriteMBs = snap.storage.diskWriteMBs;
    storage.iops = snap.storage.iops;
    storage.queueDepth = snap.storage.queueDepth;
    storage.activeTimePct = snap.storage.activeTimePct;
    storage.ready = snap.storage.ready;
    storage.wearPct = snap.storage.wearPct;
    storage.tempC = snap.storage.tempC;
    storageSamples_.push_back(storage);
    if (storageSamples_.size() > 500) storageSamples_.pop_front();

    PowerHistorySample power;
    power.timestampNs = snap.timestampNs;
    power.source = snap.power.source;
    power.batteryPresent = snap.power.batteryPresent;
    power.batteryChargePct = snap.power.batteryChargePct;
    power.batteryChargeCycles = snap.power.batteryChargeCycles;
    power.batteryCapacityMWh = snap.power.batteryCapacityMWh;
    power.batteryWearPct = snap.power.batteryWearPct;
    power.acConnected = snap.power.acConnected;
    power.acOnline = snap.power.acOnline;
    power.systemLoadW = snap.power.systemLoadW;
    power.totalPowerW = snap.power.totalPowerW;
    powerSamples_.push_back(power);
    if (powerSamples_.size() > 500) powerSamples_.pop_front();

    DetectAlerts(snap);
  }

  std::deque<CpuHistorySample> CpuHistory() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return cpuSamples_;
  }

  std::deque<GpuHistorySample> GpuHistory() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return gpuSamples_;
  }

  std::deque<ThermalHistorySample> ThermalHistory() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return thermalSamples_;
  }

  std::deque<StorageHistorySample> StorageHistory() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return storageSamples_;
  }

  std::deque<PowerHistorySample> PowerHistory() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return powerSamples_;
  }

  std::vector<HardwareAlert> RecentAlerts(int count = 20) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<HardwareAlert> result;
    int n = std::min(count, static_cast<int>(alerts_.size()));
    for (int i = static_cast<int>(alerts_.size()) - n;
         i < static_cast<int>(alerts_.size()); ++i) {
      result.push_back(alerts_[i]);
    }
    return result;
  }

  std::vector<HardwareTrend> DetectTrends() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<HardwareTrend> trends;

    if (cpuSamples_.size() >= 3) {
      std::vector<double> vals;
      for (const auto& s : cpuSamples_) vals.push_back(s.overallPct);
      auto t = ComputeTrend(vals, L"cpu.pct");
      if (t.direction != L"stable") trends.push_back(std::move(t));
    }

    if (thermalSamples_.size() >= 3) {
      std::vector<double> vals;
      for (const auto& s : thermalSamples_) vals.push_back(s.cpuCoreTempC);
      auto t = ComputeTrend(vals, L"thermal.cpuCoreTempC");
      if (t.direction != L"stable") trends.push_back(std::move(t));
    }

    if (gpuSamples_.size() >= 3) {
      std::vector<double> vals;
      for (const auto& s : gpuSamples_) vals.push_back(s.tempC);
      auto t = ComputeTrend(vals, L"gpu.tempC");
      if (t.direction != L"stable") trends.push_back(std::move(t));
    }

    if (powerSamples_.size() >= 3 && powerSamples_.front().batteryPresent) {
      std::vector<double> vals;
      for (const auto& s : powerSamples_) vals.push_back(s.batteryChargePct);
      auto t = ComputeTrend(vals, L"power.batteryChargePct");
      if (t.direction != L"stable") trends.push_back(std::move(t));
    }

    return trends;
  }

  double AvgCpu() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (cpuSamples_.empty()) return 0.0;
    double sum = 0;
    for (const auto& s : cpuSamples_) sum += s.overallPct;
    return sum / cpuSamples_.size();
  }

  double MaxCpu() const {
    std::lock_guard<std::mutex> lock(mutex_);
    double maxVal = 0;
    for (const auto& s : cpuSamples_) {
      if (s.overallPct > maxVal) maxVal = s.overallPct;
    }
    return maxVal;
  }

  double AvgTemp() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (thermalSamples_.empty()) return 0.0;
    double sum = 0;
    for (const auto& s : thermalSamples_) sum += s.cpuCoreTempC;
    return sum / thermalSamples_.size();
  }

  double MaxTemp() const {
    std::lock_guard<std::mutex> lock(mutex_);
    double maxVal = 0;
    for (const auto& s : thermalSamples_) {
      if (s.cpuCoreTempC > maxVal) maxVal = s.cpuCoreTempC;
    }
    return maxVal;
  }

  void Clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    cpuSamples_.clear();
    gpuSamples_.clear();
    thermalSamples_.clear();
    storageSamples_.clear();
    powerSamples_.clear();
    alerts_.clear();
  }

private:
  void DetectAlerts(const SystemSnapshot& snap) {
    if (snap.thermal.cpuCoreTempC > 85.0) {
      HardwareAlert a;
      a.timestampNs = snap.timestampNs;
      a.component = L"cpu";
      a.type = L"THERMAL_CRITICAL";
      a.description = L"CPU temp " +
        std::to_wstring(static_cast<int>(snap.thermal.cpuCoreTempC)) + L"C";
      a.value = snap.thermal.cpuCoreTempC;
      a.threshold = 85.0;
      a.severity = EventSeverity::Critical;
      alerts_.push_back(std::move(a));
    } else if (snap.thermal.cpuCoreTempC > 75.0) {
      HardwareAlert a;
      a.timestampNs = snap.timestampNs;
      a.component = L"cpu";
      a.type = L"THERMAL_HIGH";
      a.description = L"CPU temp " +
        std::to_wstring(static_cast<int>(snap.thermal.cpuCoreTempC)) + L"C";
      a.value = snap.thermal.cpuCoreTempC;
      a.threshold = 75.0;
      a.severity = EventSeverity::High;
      alerts_.push_back(std::move(a));
    }

    if (snap.thermal.cpuThrottling) {
      HardwareAlert a;
      a.timestampNs = snap.timestampNs;
      a.component = L"cpu";
      a.type = L"THROTTLING";
      a.description = L"CPU throttling active";
      a.severity = EventSeverity::High;
      alerts_.push_back(std::move(a));
    }

    if (snap.gpu.pctValid && snap.gpu.tempC > 90.0) {
      HardwareAlert a;
      a.timestampNs = snap.timestampNs;
      a.component = L"gpu";
      a.type = L"THERMAL_CRITICAL";
      a.description = L"GPU temp " +
        std::to_wstring(static_cast<int>(snap.gpu.tempC)) + L"C";
      a.value = snap.gpu.tempC;
      a.threshold = 90.0;
      a.severity = EventSeverity::Critical;
      alerts_.push_back(std::move(a));
    }

    if (snap.storage.wearPct > 80.0) {
      HardwareAlert a;
      a.timestampNs = snap.timestampNs;
      a.component = L"storage";
      a.type = L"HIGH_WEAR";
      a.description = L"SSD wear " +
        std::to_wstring(static_cast<int>(snap.storage.wearPct)) + L"%";
      a.value = snap.storage.wearPct;
      a.threshold = 80.0;
      a.severity = EventSeverity::High;
      alerts_.push_back(std::move(a));
    }
  }

  static HardwareTrend ComputeTrend(const std::vector<double>& vals,
                                     const std::wstring& metric) {
    HardwareTrend t;
    t.metric = metric;
    if (vals.size() < 3) return t;

    double sumX = 0, sumY = 0, sumXY = 0, sumX2 = 0;
    int n = static_cast<int>(vals.size());
    for (int i = 0; i < n; ++i) {
      sumX += i;
      sumY += vals[i];
      sumXY += i * vals[i];
      sumX2 += i * i;
    }

    double denom = n * sumX2 - sumX * sumX;
    if (denom > 0) {
      t.slope = (n * sumXY - sumX * sumY) / denom;
      double mean = sumY / n;
      double ssRes = 0, ssTot = 0;
      for (int i = 0; i < n; ++i) {
        double pred = mean + t.slope * (i - sumX / n);
        ssRes += (vals[i] - pred) * (vals[i] - pred);
        ssTot += (vals[i] - mean) * (vals[i] - mean);
      }
      t.confidence = (ssTot > 0) ? 1.0 - (ssRes / ssTot) : 0.0;
      if (t.confidence < 0) t.confidence = 0;
    }

    t.currentValue = vals.back();
    t.predictedValue = t.currentValue + t.slope * 5;

    if (t.slope > 1.0 && t.confidence > 0.5) {
      t.direction = L"rising";
      t.description = metric + L" rising (+" +
        std::to_wstring(static_cast<int>(t.slope)) + L"/sample)";
    } else if (t.slope < -1.0 && t.confidence > 0.5) {
      t.direction = L"falling";
      t.description = metric + L" falling (" +
        std::to_wstring(static_cast<int>(t.slope)) + L"/sample)";
    } else {
      t.direction = L"stable";
      t.confidence = 0;
    }

    return t;
  }

  std::deque<CpuHistorySample> cpuSamples_;
  std::deque<GpuHistorySample> gpuSamples_;
  std::deque<ThermalHistorySample> thermalSamples_;
  std::deque<StorageHistorySample> storageSamples_;
  std::deque<PowerHistorySample> powerSamples_;
  std::vector<HardwareAlert> alerts_;
  mutable std::mutex mutex_;
};

}
