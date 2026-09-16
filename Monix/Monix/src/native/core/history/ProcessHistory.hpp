#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "../snapshot/ProcessTable.hpp"

namespace monix {

struct ProcessSnapshot {
  uint64_t timestampNs = 0;
  uint64_t snapshotId = 0;
  double cpuPct = 0.0;
  uint64_t ramBytes = 0;
  int threadCount = 0;
  int handleCount = 0;
  double iops = 0.0;
  double netBytesSec = 0.0;
  int priority = 0;
};

struct ProcessRecord {
  int pid = 0;
  std::wstring name;
  std::wstring path;
  std::wstring commandLine;
  int parentPid = 0;
  int sessionId = 0;
  std::wstring sessionName;
  int bitness = 0;
  uint64_t firstSeenNs = 0;
  uint64_t lastSeenNs = 0;
  bool alive = true;
  std::deque<ProcessSnapshot> history;
  double peakCpu = 0.0;
  uint64_t peakRam = 0;
  double avgCpu = 0.0;
  double avgRam = 0.0;
  int lifetimeSeconds = 0;
  int crashCount = 0;
};

struct ProcessTrend {
  int pid = 0;
  std::wstring name;
  std::wstring trendType;
  double slope = 0.0;
  double currentValue = 0.0;
  double predictedValue = 0.0;
  double confidence = 0.0;
  std::wstring description;
};

class ProcessHistory {
public:
  void RecordSnapshot(const ProcessTable& table, uint64_t timestampNs, uint64_t snapshotId) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::map<int, bool> seen;

    for (const auto& proc : table.list) {
      seen[proc.pid] = true;
      auto it = processes_.find(proc.pid);
      if (it == processes_.end()) {
        ProcessRecord rec;
        rec.pid = proc.pid;
        rec.name = proc.name;
        rec.parentPid = proc.parentPid;
        rec.firstSeenNs = timestampNs;
        rec.lastSeenNs = timestampNs;
        it = processes_.insert({proc.pid, std::move(rec)}).first;
      }

      auto& rec = it->second;
      rec.lastSeenNs = timestampNs;
      rec.alive = true;
      if (!proc.path.empty()) rec.path = proc.path;

      ProcessSnapshot ps;
      ps.timestampNs = timestampNs;
      ps.snapshotId = snapshotId;
      ps.cpuPct = proc.cpuPct;
      ps.ramBytes = proc.ramBytes;
      ps.threadCount = proc.threadCount;
      ps.handleCount = proc.handleCount;
      ps.iops = proc.iops;
      ps.netBytesSec = proc.netBytesSec;
      ps.priority = proc.priority;

      rec.history.push_back(ps);
      if (rec.history.size() > 200) rec.history.pop_front();

      if (proc.cpuPct > rec.peakCpu) rec.peakCpu = proc.cpuPct;
      if (proc.ramBytes > rec.peakRam) rec.peakRam = proc.ramBytes;

      double dt = static_cast<double>(timestampNs - rec.firstSeenNs) / 1e9;
      if (dt > 0) {
        double sumCpu = 0;
        uint64_t sumRam = 0;
        for (const auto& h : rec.history) {
          sumCpu += h.cpuPct;
          sumRam += h.ramBytes;
        }
        rec.avgCpu = sumCpu / rec.history.size();
        rec.avgRam = sumRam / rec.history.size();
        rec.lifetimeSeconds = static_cast<int>(dt);
      }
    }

    for (auto& [pid, rec] : processes_) {
      if (seen.find(pid) == seen.end()) {
        rec.alive = false;
      }
    }
  }

  ProcessRecord GetProcess(int pid) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = processes_.find(pid);
    return (it != processes_.end()) ? it->second : ProcessRecord{};
  }

  std::vector<ProcessRecord> AliveProcesses() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<ProcessRecord> result;
    for (const auto& [pid, rec] : processes_) {
      if (rec.alive) result.push_back(rec);
    }
    return result;
  }

  std::vector<ProcessRecord> DeadProcesses() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<ProcessRecord> result;
    for (const auto& [pid, rec] : processes_) {
      if (!rec.alive) result.push_back(rec);
    }
    return result;
  }

  std::vector<ProcessRecord> ProcessesByName(const std::wstring& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<ProcessRecord> result;
    for (const auto& [pid, rec] : processes_) {
      if (rec.name == name) result.push_back(rec);
    }
    return result;
  }

  std::vector<ProcessRecord> TopCpuProcesses(int count = 10) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<ProcessRecord> alive;
    for (const auto& [pid, rec] : processes_) {
      if (rec.alive) alive.push_back(rec);
    }
    std::sort(alive.begin(), alive.end(),
      [](const ProcessRecord& a, const ProcessRecord& b) {
        return a.avgCpu > b.avgCpu;
      });
    if (static_cast<int>(alive.size()) > count) alive.resize(count);
    return alive;
  }

  std::vector<ProcessRecord> TopRamProcesses(int count = 10) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<ProcessRecord> alive;
    for (const auto& [pid, rec] : processes_) {
      if (rec.alive) alive.push_back(rec);
    }
    std::sort(alive.begin(), alive.end(),
      [](const ProcessRecord& a, const ProcessRecord& b) {
        return a.avgRam > b.avgRam;
      });
    if (static_cast<int>(alive.size()) > count) alive.resize(count);
    return alive;
  }

  std::vector<ProcessRecord> HighCpuProcesses(double threshold = 50.0) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<ProcessRecord> result;
    for (const auto& [pid, rec] : processes_) {
      if (rec.alive && rec.avgCpu > threshold) result.push_back(rec);
    }
    return result;
  }

  std::vector<ProcessTrend> DetectTrends() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<ProcessTrend> trends;

    for (const auto& [pid, rec] : processes_) {
      if (!rec.alive || rec.history.size() < 3) continue;

      std::vector<double> cpuVals;
      for (const auto& h : rec.history) cpuVals.push_back(h.cpuPct);

      ProcessTrend t;
      t.pid = pid;
      t.name = rec.name;
      t.currentValue = cpuVals.back();

      double sumX = 0, sumY = 0, sumXY = 0, sumX2 = 0;
      int n = static_cast<int>(cpuVals.size());
      for (int i = 0; i < n; ++i) {
        sumX += i;
        sumY += cpuVals[i];
        sumXY += i * cpuVals[i];
        sumX2 += i * i;
      }

      double denom = n * sumX2 - sumX * sumX;
      if (denom > 0) {
        t.slope = (n * sumXY - sumX * sumY) / denom;
        double mean = sumY / n;
        double ssRes = 0, ssTot = 0;
        for (int i = 0; i < n; ++i) {
          double pred = (sumY / n) + t.slope * (i - sumX / n);
          ssRes += (cpuVals[i] - pred) * (cpuVals[i] - pred);
          ssTot += (cpuVals[i] - mean) * (cpuVals[i] - mean);
        }
        t.confidence = (ssTot > 0) ? 1.0 - (ssRes / ssTot) : 0.0;
        if (t.confidence < 0) t.confidence = 0;
      }

      t.predictedValue = t.currentValue + t.slope * 5;

      if (t.slope > 2.0 && t.confidence > 0.5) {
        t.trendType = L"rising";
        t.description = t.name + L" CPU rising (+"
          + std::to_wstring(static_cast<int>(t.slope)) + L"%/sample)";
      } else if (t.slope < -2.0 && t.confidence > 0.5) {
        t.trendType = L"falling";
        t.description = t.name + L" CPU falling ("
          + std::to_wstring(static_cast<int>(t.slope)) + L"%/sample)";
      } else {
        t.trendType = L"stable";
        t.confidence = 0;
      }

      if (t.trendType != L"stable") trends.push_back(std::move(t));
    }

    std::sort(trends.begin(), trends.end(),
      [](const ProcessTrend& a, const ProcessTrend& b) {
        return std::abs(a.slope) > std::abs(b.slope);
      });

    return trends;
  }

  std::map<int, int> NewProcessesSince(uint64_t sinceNs) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::map<int, int> result;
    for (const auto& [pid, rec] : processes_) {
      if (rec.firstSeenNs >= sinceNs && rec.alive) {
        result[pid] = rec.parentPid;
      }
    }
    return result;
  }

  std::map<int, int> DeadProcessesSince(uint64_t sinceNs) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::map<int, int> result;
    for (const auto& [pid, rec] : processes_) {
      if (!rec.alive && rec.lastSeenNs >= sinceNs) {
        result[pid] = rec.parentPid;
      }
    }
    return result;
  }

  int AliveCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    int count = 0;
    for (const auto& [pid, rec] : processes_) {
      if (rec.alive) count++;
    }
    return count;
  }

  int DeadCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    int count = 0;
    for (const auto& [pid, rec] : processes_) {
      if (!rec.alive) count++;
    }
    return count;
  }

  void Clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    processes_.clear();
  }

private:
  std::map<int, ProcessRecord> processes_;
  mutable std::mutex mutex_;
};

}
