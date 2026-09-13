#include "NetworkTrafficRule.hpp"

#include "../../telemetry/Snapshot.hpp"

#include <set>
#include <string>

namespace monix {

void NetworkTrafficRule::Evaluate(const Snapshot& current,
                                  const Snapshot* previous,
                                  std::vector<ScramFinding>& findings) {
  if (!previous) return;
  const Snapshot& prev = *previous;

  const int inboundDelta = current.inboundConnections - prev.inboundConnections;
  if (inboundDelta > 20) {
    findings.push_back({
      L"Inbound connection burst detected.",
      L"Anomalous increase in inbound socket count.",
      L"Inbound connection burst: +" + std::to_wstring(inboundDelta) + L" sockets.",
      6
    });
  }

  const int outboundDelta = current.outboundConnections - prev.outboundConnections;
  if (outboundDelta > 30) {
    findings.push_back({
      L"Outbound connection burst detected.",
      L"Anomalous increase in outbound socket count.",
      L"Outbound connection burst: +" + std::to_wstring(outboundDelta) + L" sockets.",
      8
    });
  }

  if (outboundDelta < -30) {
    findings.push_back({
      L"Connection teardown spike detected.",
      L"Large number of outbound connections closed.",
      L"Connection teardown spike: " + std::to_wstring(outboundDelta) + L" sockets.",
      4
    });
  }

  if (current.tcpResets > prev.tcpResets && prev.tcpResets > 0) {
    const uint64_t resetDelta = current.tcpResets - prev.tcpResets;
    if (resetDelta > 50) {
      findings.push_back({
        L"TCP reset spike detected.",
        L"Abnormal number of TCP connections in terminal states.",
        L"TCP resets: +" + std::to_wstring(resetDelta) + L" in one sample.",
        12
      });
    }
  }

  if (current.tcpRetransmits > prev.tcpRetransmits && prev.tcpRetransmits > 0) {
    const uint64_t retransDelta = current.tcpRetransmits - prev.tcpRetransmits;
    if (retransDelta > 40) {
      findings.push_back({
        L"TCP retransmission spike detected.",
        L"High retransmission rate indicates packet loss or congestion.",
        L"TCP retransmits: +" + std::to_wstring(retransDelta),
        12
      });
    }
  }

  if (current.netPrimaryLinkSpeedBps > 0) {
    double utilization = 0.0;
    if (current.netDownBytesPerSec + current.netUpBytesPerSec > 0) {
      utilization = static_cast<double>(current.netDownBytesPerSec + current.netUpBytesPerSec) * 8.0 /
                    static_cast<double>(current.netPrimaryLinkSpeedBps) * 100.0;
    }
    const bool saturated = utilization >= 85.0;
    if (saturated && !prevBandwidthSaturation_) {
      findings.push_back({
        L"Bandwidth saturation detected.",
        L"Aggregate throughput is near the link capacity.",
        L"Link utilization: " + std::to_wstring(static_cast<int>(utilization)) + L"%",
        12
      });
    }
    prevBandwidthSaturation_ = saturated;
  }

  const int udpDelta = current.udpConnectionCount - prev.udpConnectionCount;
  if (udpDelta < -40) {
    findings.push_back({
      L"UDP loss spike detected.",
      L"Significant drop in active UDP connections.",
      L"UDP connections dropped by " + std::to_wstring(-udpDelta) + L".",
      10
    });
  }

  if (current.outboundConnections < prev.outboundConnections * 0.5 && prev.outboundConnections > 20) {
    findings.push_back({
      L"Firewall drop event detected.",
      L"Established connections dropped sharply without local action.",
      L"Connection collapse: " + std::to_wstring(prev.outboundConnections) + L" -> " + std::to_wstring(current.outboundConnections),
      18
    });
  }

  std::set<std::wstring> currentRemotePorts;
  for (const auto& flow : current.flows) {
    if (flow.state == L"ACTIVE") {
      auto colon = flow.remote.find(L':');
      if (colon != std::wstring::npos) {
        currentRemotePorts.insert(flow.remote.substr(0, colon));
      }
    }
  }
  const bool portScan = currentRemotePorts.size() > 8;
  if (portScan && !prevPortScan_) {
    findings.push_back({
      L"Port scan pattern detected.",
      L"Abnormally many unique remote endpoints contacted.",
      L"Port scan pattern: " + std::to_wstring(currentRemotePorts.size()) + L" unique remote endpoints.",
      8
    });
  }
  prevPortScan_ = portScan;

  const bool networkPressure = current.latencyMs >= 45 || current.outboundConnections >= 180;
  if (networkPressure && !prevNetworkPressure_) {
    findings.push_back({
      L"Network latency or socket fan-out is above the nominal baseline.",
      L"Aggregate network pressure exceeds the safe envelope.",
      L"Network latency or socket fan-out is above the nominal baseline.",
      10
    });
  }
  prevNetworkPressure_ = networkPressure;
}

} // namespace monix
