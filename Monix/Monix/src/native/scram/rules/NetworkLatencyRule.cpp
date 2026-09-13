#include "NetworkLatencyRule.hpp"

#include "../../telemetry/Snapshot.hpp"

#include <cmath>

namespace monix {

void NetworkLatencyRule::Evaluate(const Snapshot& current,
                                  const Snapshot* previous,
                                  std::vector<ScramFinding>& findings) {
  if (!previous) return;
  const Snapshot& prev = *previous;

  if (current.pingRttMs >= 0 && prev.pingRttMs >= 0) {
    if (current.pingRttMs - prev.pingRttMs > 50) {
      findings.push_back({
        L"Latency spike detected.",
        L"ICMP probe RTT jumped significantly.",
        L"Latency spike: " + std::to_wstring(prev.pingRttMs) + L"ms -> " + std::to_wstring(current.pingRttMs) + L"ms",
        10
      });
    }
  }

  if (current.pingRttMs >= 0) {
    pingRttSamples_.push_back(current.pingRttMs);
    if (pingRttSamples_.size() > 20) pingRttSamples_.erase(pingRttSamples_.begin());
    if (pingRttSamples_.size() >= 5) {
      double sum = 0.0;
      for (int v : pingRttSamples_) sum += v;
      double mean = sum / pingRttSamples_.size();
      double varSum = 0.0;
      for (int v : pingRttSamples_) {
        double d = v - mean;
        varSum += d * d;
      }
      pingJitterStddev_ = std::sqrt(varSum / pingRttSamples_.size());
      if (pingJitterStddev_ > 25.0) {
        findings.push_back({
          L"Jitter spike detected.",
          L"ICMP probe latency variance exceeds the nominal envelope.",
          L"Jitter stddev: " + std::to_wstring(static_cast<int>(pingJitterStddev_)) + L"ms",
          10
        });
      }
    }
  }

  if (current.pingRttMs < 0 && prev.pingRttMs >= 0) {
    std::wstring diag = L"Gateway probe failed (was " + std::to_wstring(prev.pingRttMs) + L"ms).";
    int risk = 16;
    if (current.tcpResets > prev.tcpResets) {
      diag += L" TCP resets also increased.";
      risk = 20;
    }
    findings.push_back({
      L"Gateway unreachable detected.",
      L"ICMP probe to default gateway failed.",
      diag,
      risk
    });
  }

  if (current.dnsResolutionOk == 0 && prev.dnsResolutionOk == 1) {
    findings.push_back({
      L"DNS resolution failure detected.",
      L"DNS probe to google.com failed.",
      L"DNS probe returned failure.",
      16
    });
  }

  if (current.dnsResolutionMs >= 0 && prev.dnsResolutionMs >= 0) {
    if (current.dnsResolutionMs - prev.dnsResolutionMs > 100) {
      findings.push_back({
        L"DNS latency spike detected.",
        L"DNS resolution time increased significantly.",
        L"DNS spike: " + std::to_wstring(prev.dnsResolutionMs) + L"ms -> " + std::to_wstring(current.dnsResolutionMs) + L"ms",
        10
      });
    }
  }
}

} // namespace monix
