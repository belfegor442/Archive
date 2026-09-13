#include "NetworkTopologyRule.hpp"

#include "../../telemetry/Snapshot.hpp"
#include <iptypes.h>

namespace monix {

void NetworkTopologyRule::Evaluate(const Snapshot& current,
                                   const Snapshot* previous,
                                   std::vector<ScramFinding>& findings) {
  if (!previous) return;
  const Snapshot& prev = *previous;

  if (current.netAdapterCount == 0 || prev.netAdapterCount == 0) return;

  for (const auto& addr : current.netAdapterAddresses) {
    if (prev.netAdapterAddresses.find(addr) == prev.netAdapterAddresses.end()) {
      findings.push_back({
        L"IP address change detected: new " + addr,
        L"A new IP address has appeared on a network adapter.",
        L"Root cause candidate: DHCP renewal or manual reconfiguration.",
        14
      });
    }
  }

  for (const auto& addr : prev.netAdapterAddresses) {
    if (current.netAdapterAddresses.find(addr) == current.netAdapterAddresses.end()) {
      findings.push_back({
        L"IP address removed: " + addr,
        L"A previously active IP address is no longer present.",
        L"Root cause candidate: adapter reconfiguration or cable event.",
        10
      });
    }
  }

  for (const auto& gw : current.netAdapterGateways) {
    if (prev.netAdapterGateways.find(gw) == prev.netAdapterGateways.end()) {
      findings.push_back({
        L"Gateway change detected: " + gw,
        L"A new default gateway has appeared.",
        L"Root cause candidate: route table modification or failover.",
        12
      });
    }
  }

  if (prev.netAdapterOperStatuses.size() > 0 && current.netAdapterOperStatuses.size() > 0) {
    for (const auto& status : current.netAdapterOperStatuses) {
      if (status == 1 && prev.netAdapterOperStatuses.find(1) == prev.netAdapterOperStatuses.end() && prev.netAdapterOperStatuses.size() > 0) {
        findings.push_back({
          L"Network link up event detected.",
          L"A network adapter has transitioned to operational state.",
          L"Link up: adapter became operational.",
          6
        });
      }
    }
    for (const auto& status : prev.netAdapterOperStatuses) {
      if (status == 1 && current.netAdapterOperStatuses.find(1) == current.netAdapterOperStatuses.end() && current.netAdapterOperStatuses.size() > 0) {
        findings.push_back({
          L"Network link down event detected.",
          L"A network adapter has gone offline.",
          L"Link down: adapter lost operational status.",
          14
        });
      }
    }
  }

  for (const auto& speed : current.netAdapterSpeeds) {
    if (speed > 0 && prev.netAdapterSpeeds.find(speed) == prev.netAdapterSpeeds.end() && prev.netAdapterSpeeds.size() > 0 && current.netAdapterCount > 0) {
      bool isWifiType = false;
      for (const auto& t : current.netAdapterTypes) {
        if (t == IF_TYPE_IEEE80211) { isWifiType = true; break; }
      }
      if (isWifiType && speed < 72000000) {
        findings.push_back({
          L"Wi-Fi signal degradation detected.",
          L"Wireless adapter speed dropped below nominal.",
          L"Wi-Fi speed: " + std::to_wstring(speed / 1000000) + L" Mbps (degraded).",
          10
        });
      } else {
        findings.push_back({
          L"Ethernet speed renegotiated.",
          L"A wired adapter changed link speed.",
          L"Ethernet speed renegotiated to " + std::to_wstring(speed / 1000000) + L" Mbps.",
          4
        });
      }
    }
  }

  if (prev.netAdapterOperStatuses.size() > 0 && current.netAdapterOperStatuses.size() == 0) {
    findings.push_back({
      L"Network interface reset detected.",
      L"Adapters bounced without a full outage.",
      L"Network interface reset detected: adapters bounced.",
      12
    });
  }

  if (current.routeTableHash != 0 && prev.routeTableHash != 0 && current.routeTableHash != prev.routeTableHash) {
    findings.push_back({
      L"Route table change detected.",
      L"The IP forwarding table has changed unexpectedly.",
      L"Route table hash changed: routing may have shifted.",
      14
    });
  }

  if (prev.proxyEnabled >= 0 && current.proxyEnabled != prev.proxyEnabled) {
    findings.push_back({
      L"Proxy configuration change detected.",
      L"System proxy settings have been modified.",
      L"Proxy state changed: " + std::to_wstring(current.proxyEnabled ? 1 : 0),
      14
    });
  }
}

} // namespace monix
