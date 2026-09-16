#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <mutex>
#include <string>
#include <vector>

#include "../snapshot/NetworkState.hpp"

namespace monix {

struct NetworkSample {
  uint64_t timestampNs = 0;
  uint64_t snapshotId = 0;
  double upKbps = 0.0;
  double downKbps = 0.0;
  double rttMs = 0.0;
  int activeConns = 0;
  double droppedPktPct = 0.0;
  double tcpRetransmitPct = 0.0;
  int totalConns = 0;
};

struct NetworkInterface {
  std::wstring name;
  std::wstring description;
  std::wstring macAddress;
  std::wstring ipv4Address;
  std::wstring ipv6Address;
  std::wstring gateway;
  std::wstring dns;
  bool isUp = true;
  bool isWifi = false;
  double speedMbps = 0.0;
};

struct NetworkConnection {
  std::wstring localAddress;
  int localPort = 0;
  std::wstring remoteAddress;
  int remotePort = 0;
  std::wstring state;
  int processId = 0;
  std::wstring processName;
  std::wstring protocol;
};

struct NetworkAlert {
  uint64_t timestampNs = 0;
  std::wstring type;
  std::wstring description;
  double value = 0.0;
  double threshold = 0.0;
  EventSeverity severity = EventSeverity::Info;
};

struct NetworkTrend {
  std::wstring direction;
  double slope = 0.0;
  double currentValue = 0.0;
  double predictedValue = 0.0;
  double confidence = 0.0;
  std::wstring description;
};

class NetworkHistory {
public:
  void RecordSnapshot(const NetworkState& net, uint64_t timestampNs, uint64_t snapshotId) {
    std::lock_guard<std::mutex> lock(mutex_);

    NetworkSample sample;
    sample.timestampNs = timestampNs;
    sample.snapshotId = snapshotId;
    sample.upKbps = net.upKbps;
    sample.downKbps = net.downKbps;
    sample.rttMs = net.rttMs;
    sample.activeConns = net.activeConns;
    sample.droppedPktPct = net.droppedPktPct;
    sample.tcpRetransmitPct = net.tcpRetransmitPct;
    sample.totalConns = net.totalConns;

    samples_.push_back(sample);
    if (samples_.size() > 500) samples_.pop_front();

    DetectAlerts(sample);
    UpdateStats();
  }

  void SetInterfaces(const std::vector<NetworkInterface>& ifaces) {
    std::lock_guard<std::mutex> lock(mutex_);
    interfaces_ = ifaces;
  }

  void SetConnections(const std::vector<NetworkConnection>& conns) {
    std::lock_guard<std::mutex> lock(mutex_);
    connections_ = conns;
  }

  std::vector<NetworkSample> RecentSamples(int count = 50) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<NetworkSample> result;
    int n = std::min(count, static_cast<int>(samples_.size()));
    for (int i = static_cast<int>(samples_.size()) - n;
         i < static_cast<int>(samples_.size()); ++i) {
      result.push_back(samples_[i]);
    }
    return result;
  }

  std::vector<NetworkSample> SamplesInRange(uint64_t startNs, uint64_t endNs) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<NetworkSample> result;
    for (const auto& s : samples_) {
      if (s.timestampNs >= startNs && s.timestampNs <= endNs) {
        result.push_back(s);
      }
    }
    return result;
  }

  NetworkSample LatestSample() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return samples_.empty() ? NetworkSample{} : samples_.back();
  }

  double AvgUpKbps() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (samples_.empty()) return 0.0;
    double sum = 0;
    for (const auto& s : samples_) sum += s.upKbps;
    return sum / samples_.size();
  }

  double AvgDownKbps() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (samples_.empty()) return 0.0;
    double sum = 0;
    for (const auto& s : samples_) sum += s.downKbps;
    return sum / samples_.size();
  }

  double AvgRtt() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (samples_.empty()) return 0.0;
    double sum = 0;
    for (const auto& s : samples_) sum += s.rttMs;
    return sum / samples_.size();
  }

  double MaxRtt() const {
    std::lock_guard<std::mutex> lock(mutex_);
    double maxVal = 0;
    for (const auto& s : samples_) {
      if (s.rttMs > maxVal) maxVal = s.rttMs;
    }
    return maxVal;
  }

  double MinRtt() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (samples_.empty()) return 0.0;
    double minVal = samples_.front().rttMs;
    for (const auto& s : samples_) {
      if (s.rttMs < minVal) minVal = s.rttMs;
    }
    return minVal;
  }

  double StddevRtt() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (samples_.size() < 2) return 0.0;
    double mean = AvgRtt();
    double sumSq = 0;
    for (const auto& s : samples_) {
      double diff = s.rttMs - mean;
      sumSq += diff * diff;
    }
    return std::sqrt(sumSq / samples_.size());
  }

  std::vector<NetworkAlert> RecentAlerts(int count = 20) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<NetworkAlert> result;
    int n = std::min(count, static_cast<int>(alerts_.size()));
    for (int i = static_cast<int>(alerts_.size()) - n;
         i < static_cast<int>(alerts_.size()); ++i) {
      result.push_back(alerts_[i]);
    }
    return result;
  }

  std::vector<NetworkInterface> Interfaces() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return interfaces_;
  }

  std::vector<NetworkConnection> Connections() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return connections_;
  }

  std::vector<NetworkConnection> ConnectionsByProcess(int pid) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<NetworkConnection> result;
    for (const auto& c : connections_) {
      if (c.processId == pid) result.push_back(c);
    }
    return result;
  }

  std::vector<NetworkConnection> ConnectionsByState(const std::wstring& state) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<NetworkConnection> result;
    for (const auto& c : connections_) {
      if (c.state == state) result.push_back(c);
    }
    return result;
  }

  NetworkTrend RttTrend() const {
    std::lock_guard<std::mutex> lock(mutex_);
    NetworkTrend trend;
    if (samples_.size() < 3) return trend;

    std::vector<double> vals;
    for (const auto& s : samples_) vals.push_back(s.rttMs);

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
      trend.slope = (n * sumXY - sumX * sumY) / denom;
      double mean = sumY / n;
      double ssRes = 0, ssTot = 0;
      for (int i = 0; i < n; ++i) {
        double pred = mean + trend.slope * (i - sumX / n);
        ssRes += (vals[i] - pred) * (vals[i] - pred);
        ssTot += (vals[i] - mean) * (vals[i] - mean);
      }
      trend.confidence = (ssTot > 0) ? 1.0 - (ssRes / ssTot) : 0.0;
      if (trend.confidence < 0) trend.confidence = 0;
    }

    trend.currentValue = vals.back();
    trend.predictedValue = trend.currentValue + trend.slope * 5;

    if (trend.slope > 5.0 && trend.confidence > 0.5) {
      trend.direction = L"rising";
      trend.description = L"RTT increasing (+" +
        std::to_wstring(static_cast<int>(trend.slope)) + L"ms/sample)";
    } else if (trend.slope < -5.0 && trend.confidence > 0.5) {
      trend.direction = L"falling";
      trend.description = L"RTT decreasing (" +
        std::to_wstring(static_cast<int>(trend.slope)) + L"ms/sample)";
    } else {
      trend.direction = L"stable";
      trend.confidence = 0;
    }

    return trend;
  }

  void Clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    samples_.clear();
    alerts_.clear();
    interfaces_.clear();
    connections_.clear();
  }

  int SampleCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return static_cast<int>(samples_.size());
  }

private:
  void DetectAlerts(const NetworkSample& sample) {
    if (sample.rttMs > 200.0) {
      NetworkAlert a;
      a.timestampNs = sample.timestampNs;
      a.type = L"HIGH_LATENCY";
      a.description = L"RTT " + std::to_wstring(static_cast<int>(sample.rttMs)) + L"ms";
      a.value = sample.rttMs;
      a.threshold = 200.0;
      a.severity = EventSeverity::High;
      alerts_.push_back(std::move(a));
    }
    if (sample.droppedPktPct > 5.0) {
      NetworkAlert a;
      a.timestampNs = sample.timestampNs;
      a.type = L"HIGH_DROPS";
      a.description = L"Dropped " + std::to_wstring(static_cast<int>(sample.droppedPktPct)) + L"%";
      a.value = sample.droppedPktPct;
      a.threshold = 5.0;
      a.severity = EventSeverity::High;
      alerts_.push_back(std::move(a));
    }
    if (sample.tcpRetransmitPct > 5.0) {
      NetworkAlert a;
      a.timestampNs = sample.timestampNs;
      a.type = L"HIGH_RETRANSMIT";
      a.description = L"Retransmit " + std::to_wstring(static_cast<int>(sample.tcpRetransmitPct)) + L"%";
      a.value = sample.tcpRetransmitPct;
      a.threshold = 5.0;
      a.severity = EventSeverity::Medium;
      alerts_.push_back(std::move(a));
    }
  }

  void UpdateStats() {
  }

  std::deque<NetworkSample> samples_;
  std::vector<NetworkAlert> alerts_;
  std::vector<NetworkInterface> interfaces_;
  std::vector<NetworkConnection> connections_;
  mutable std::mutex mutex_;
};

}
