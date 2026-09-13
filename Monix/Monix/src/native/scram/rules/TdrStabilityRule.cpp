#include "TdrStabilityRule.hpp"

#include "../../telemetry/Snapshot.hpp"
#include "../../core/TextUtils.hpp"

namespace monix {

void TdrStabilityRule::Evaluate(const Snapshot& current,
                                const Snapshot* previous,
                                std::vector<ScramFinding>& findings) {
  if (!previous) return;

  if (current.gpuPctValid == 1 && current.gpuPct >= 85.0 && previous->gpuPctValid == 1 && previous->gpuPct < 85.0 && current.gpuTempC > 80.0) {
    findings.push_back({
      L"TDR warning: GPU under sustained high load.",
      L"Prolonged GPU saturation increases TDR risk.",
      (L"TDR risk: GPU at " + std::to_wstring((int)current.gpuPct) + L"% for >1 sample, temp " + FormatTemperature(current.gpuTempC, current.gpuTempEstimated)),
      16
    });
  }

  if (previous->gpuPctValid == 1 && previous->gpuPct >= 90.0 && current.gpuPctValid == 1 && current.gpuPct < 30.0) {
    findings.push_back({
      L"TDR recovery detected.",
      L"GPU utilization dropped from saturation to idle, possibly indicating driver recovery.",
      (L"TDR recovery: " + std::to_wstring((int)previous->gpuPct) + L"% -> " + std::to_wstring((int)current.gpuPct) + L"%"),
      14
    });
  }

  if (current.tdrLevel != previous->tdrLevel && previous->tdrLevel >= 0) {
    findings.push_back({
      L"PCIe link speed change or driver config change.",
      L"Graphics driver configuration has been modified.",
      (L"TDR level: " + std::to_wstring(previous->tdrLevel) + L" -> " + std::to_wstring(current.tdrLevel)),
      10
    });
  }
}

} // namespace monix
