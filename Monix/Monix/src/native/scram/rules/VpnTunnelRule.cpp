#include "VpnTunnelRule.hpp"

#include "../../telemetry/Snapshot.hpp"

namespace monix {

void VpnTunnelRule::Evaluate(const Snapshot& current,
                             const Snapshot* previous,
                             std::vector<ScramFinding>& findings) {
  if (!previous) return;
  const Snapshot& prev = *previous;

  if (!prev.vpnAdapterDescriptions.empty() && current.vpnAdapterDescriptions.empty()) {
    findings.push_back({
      L"VPN disconnect event detected.",
      L"A VPN or tunnel adapter has been removed.",
      L"VPN adapter disappeared from interface list.",
      14
    });
  } else if (prev.vpnAdapterDescriptions.empty() && !current.vpnAdapterDescriptions.empty()) {
    findings.push_back({
      L"VPN connect event detected.",
      L"A VPN or tunnel adapter has been activated.",
      L"New VPN adapter detected in interface list.",
      6
    });
  }
}

} // namespace monix
