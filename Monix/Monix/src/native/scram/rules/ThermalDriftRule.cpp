#include "ThermalDriftRule.hpp"

#include "../../telemetry/Snapshot.hpp"
#include "../../core/TextUtils.hpp"

namespace monix {

void ThermalDriftRule::Evaluate(const Snapshot& current,
                                const Snapshot* previous,
                                std::vector<ScramFinding>& findings) {
  const bool cpuHot = current.cpuCoreTempC >= 78.0;
  const bool gpuHot = current.gpuTempC >= 82.0;
  const bool storageHot = current.storageTempC >= 56.0;
  const bool anyHot = cpuHot || gpuHot || storageHot;

  bool wasHot = false;
  if (previous) {
    wasHot = (previous->cpuCoreTempC >= 78.0) || (previous->gpuTempC >= 82.0) || (previous->storageTempC >= 56.0);
  }

  if (anyHot && !wasHot) {
    findings.push_back({
      L"Thermal drift detected on active hardware.",
      L"At least one sensor or estimate crossed the warm operating envelope.",
      L"Thermal watch is elevated. Review cooling and active tasks.",
      16
    });
  }
}

} // namespace monix
