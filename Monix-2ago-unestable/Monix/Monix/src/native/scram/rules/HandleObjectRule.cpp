#include "HandleObjectRule.hpp"

#include "../../telemetry/Snapshot.hpp"

namespace monix {

void HandleObjectRule::Evaluate(const Snapshot& current,
                                const Snapshot* previous,
                                std::vector<ScramFinding>& findings) {
  if (previous) {
    const int handleDelta = current.handleCount - previous->handleCount;
    if (handleDelta > 10000) {
      findings.push_back({
        (L"Handle surge detected: +" + std::to_wstring(handleDelta) + L" handles"),
        L"Rapid handle allocation may indicate a resource leak.",
        (L"Handle count jumped by " + std::to_wstring(handleDelta) + L" in one sample."),
        12
      });
    } else if (handleDelta < -10000) {
      findings.push_back({
        (L"Handle mass release: " + std::to_wstring(-handleDelta) + L" handles"),
        L"Large number of handles released in one sample.",
        (L"Handle count dropped by " + std::to_wstring(-handleDelta) + L" (mass release)."),
        4
      });
    }
  }
}

} // namespace monix
