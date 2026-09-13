#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdint>
#include <string>
#include <vector>

namespace monix::telemetry {

struct UeoSignal {
  std::wstring type;
  std::wstring detail;
  double value = 0.0;
  std::uint64_t timestampNs = 0;
};

struct UeoIncident {
  std::wstring correlationId;
  std::wstring rootCause;
  std::wstring summary;
  std::vector<UeoSignal> signals;
  int severity = 0;
  std::uint64_t startNs = 0;
  std::uint64_t endNs = 0;
};

class UeoCorrelator {
public:
  void AddSignal(const UeoSignal& signal) {
    signals_.push_back(signal);
  }

  void SetWindowNs(std::uint64_t windowNs) { windowNs_ = windowNs; }

  bool ShouldFlush(std::uint64_t nowNs) const {
    if (signals_.empty()) return false;
    return (nowNs - signals_.front().timestampNs) >= windowNs_;
  }

  UeoIncident Flush(std::uint64_t nowNs, std::uint64_t nextIncidentId) {
    UeoIncident incident;
    incident.correlationId = L"UEO-" + std::to_wstring(nextIncidentId);
    incident.startNs = signals_.front().timestampNs;
    incident.endNs = nowNs;
    incident.signals = std::move(signals_);
    signals_.clear();

    int signalCount = static_cast<int>(incident.signals.size());
    bool hasCpuSpike = false;
    bool hasNetworkSpike = false;
    bool hasNewProcess = false;
    bool hasThermal = false;
    double maxCpu = 0.0;
    std::wstring topProcess;

    for (const auto& sig : incident.signals) {
      if (sig.type == L"cpu_spike") { hasCpuSpike = true; if (sig.value > maxCpu) maxCpu = sig.value; }
      if (sig.type == L"network_spike") hasNetworkSpike = true;
      if (sig.type == L"process_created") { hasNewProcess = true; topProcess = sig.detail; }
      if (sig.type == L"thermal") hasThermal = true;
    }

    if (hasCpuSpike && hasNewProcess && hasNetworkSpike) {
      incident.rootCause = L"process_resource_exfil";
      incident.summary = L"Process " + topProcess + L" consuming CPU and network — possible exfiltration";
      incident.severity = 2;
    } else if (hasCpuSpike && hasNewProcess) {
      incident.rootCause = L"process_cpu_burst";
      incident.summary = L"Process " + topProcess + L" causing CPU spike";
      incident.severity = 1;
    } else if (hasCpuSpike && hasNetworkSpike) {
      incident.rootCause = L"cpu_network_correlated";
      incident.summary = L"CPU and network spike correlated — unusual workload";
      incident.severity = 1;
    } else if (hasCpuSpike && hasThermal) {
      incident.rootCause = L"thermal_throttle_risk";
      incident.summary = L"CPU spike with elevated temperature — throttle risk";
      incident.severity = 2;
    } else if (hasCpuSpike) {
      incident.rootCause = L"cpu_spike_isolated";
      incident.summary = L"CPU spike without correlated signals";
      incident.severity = 0;
    } else if (signalCount >= 3) {
      incident.rootCause = L"multi_signal_anomaly";
      incident.summary = std::to_wstring(signalCount) + L" correlated signals detected";
      incident.severity = 1;
    } else {
      incident.rootCause = L"single_signal";
      incident.summary = L"Isolated signal: " + incident.signals.front().type;
      incident.severity = 0;
    }

    return incident;
  }

  void Clear() { signals_.clear(); }
  bool IsEmpty() const { return signals_.empty(); }

private:
  std::vector<UeoSignal> signals_;
  std::uint64_t windowNs_ = 30000000000ULL;
};

}
