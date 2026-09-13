#include "DisplayConfigRule.hpp"

#include "../../telemetry/Snapshot.hpp"

namespace monix {

void DisplayConfigRule::Evaluate(const Snapshot& current,
                                 const Snapshot* previous,
                                 std::vector<ScramFinding>& findings) {
  if (!previous) return;

  if (current.displayRefreshRateHz != previous->displayRefreshRateHz && previous->displayRefreshRateHz > 0) {
    findings.push_back({
      L"Display refresh rate change detected.",
      L"Monitor refresh rate has been modified.",
      (L"Refresh: " + std::to_wstring(previous->displayRefreshRateHz) + L"Hz -> " + std::to_wstring(current.displayRefreshRateHz) + L"Hz"),
      14
    });
  }

  if ((current.displayWidth != previous->displayWidth || current.displayHeight != previous->displayHeight) && previous->displayWidth > 0) {
    findings.push_back({
      L"Resolution change detected.",
      L"Display resolution has been modified.",
      (L"Resolution: " + std::to_wstring(previous->displayWidth) + L"x" + std::to_wstring(previous->displayHeight) + L" -> " + std::to_wstring(current.displayWidth) + L"x" + std::to_wstring(current.displayHeight)),
      14
    });
  }

  if (current.displayMonitorCount != previous->displayMonitorCount && previous->displayMonitorCount > 0) {
    if (current.displayMonitorCount > previous->displayMonitorCount) {
      findings.push_back({
        L"Display hotplug detected.",
        L"New display has been connected.",
        (L"Monitors: " + std::to_wstring(previous->displayMonitorCount) + L" -> " + std::to_wstring(current.displayMonitorCount)),
        6
      });
    } else {
      findings.push_back({
        L"Multi-monitor topology change detected.",
        L"Display configuration has changed.",
        (L"Monitors: " + std::to_wstring(previous->displayMonitorCount) + L" -> " + std::to_wstring(current.displayMonitorCount)),
        10
      });
    }
  }

  if (current.displayBitsPerPel != previous->displayBitsPerPel && previous->displayBitsPerPel > 0) {
    findings.push_back({
      L"Color depth change detected.",
      L"Display color depth has been modified.",
      (L"Color depth: " + std::to_wstring(previous->displayBitsPerPel) + L" -> " + std::to_wstring(current.displayBitsPerPel) + L" bpp"),
      14
    });
  }

  if (current.hdrEnabled != previous->hdrEnabled && previous->hdrEnabled >= 0) {
    findings.push_back({
      L"HDR state change detected.",
      L"Display HDR mode has been toggled.",
      (L"HDR: " + std::to_wstring(previous->hdrEnabled ? 1 : 0) + L" -> " + std::to_wstring(current.hdrEnabled ? 1 : 0)),
      14
    });
  }

  if (current.vsyncEnabled != previous->vsyncEnabled && previous->vsyncEnabled >= 0) {
    findings.push_back({
      L"VSync state change detected.",
      L"Vertical sync has been toggled.",
      (L"VSync: " + std::to_wstring(previous->vsyncEnabled ? 1 : 0) + L" -> " + std::to_wstring(current.vsyncEnabled ? 1 : 0)),
      10
    });
  }

  if (current.desktopCompositionEnabled != previous->desktopCompositionEnabled && previous->desktopCompositionEnabled >= 0) {
    findings.push_back({
      L"Desktop composition change detected.",
      L"DWM composition has been toggled.",
      (L"DWM: " + std::to_wstring(previous->desktopCompositionEnabled ? 1 : 0) + L" -> " + std::to_wstring(current.desktopCompositionEnabled ? 1 : 0)),
      14
    });
  }
}

} // namespace monix
