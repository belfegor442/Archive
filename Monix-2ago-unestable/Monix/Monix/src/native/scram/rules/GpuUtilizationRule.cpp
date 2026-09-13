#include "GpuUtilizationRule.hpp"

#include "../../telemetry/Snapshot.hpp"

namespace monix {

void GpuUtilizationRule::Evaluate(const Snapshot& current,
                                   const Snapshot* previous,
                                   std::vector<ScramFinding>& findings) {
  if (!current.gpuPctValid) {
    return;
  }

  if (current.gpuPct >= 85.0 && current.gpuPct < 95.0) {
    findings.push_back({
      L"GPU pipeline is operating near saturation.",
      L"Likely render, shader, game scene or capture workload active.",
      L"Render path sustained above 85% utilization.",
      20
    });
  }

  if (previous) {
    if (current.gpuPct > 95.0 && previous->gpuPctValid == 1 && previous->gpuPct < 50.0) {
      findings.push_back({
        L"GPU driver reset suspected.",
        L"GPU utilization spiked from low to maximum, possibly indicating a driver recovery.",
        (L"GPU reset pattern: " + std::to_wstring((int)previous->gpuPct) + L"% -> " + std::to_wstring((int)current.gpuPct) + L"%"),
        16
      });
    }

    if (current.gpuPct >= 95.0) {
      findings.push_back({
        L"GPU utilization spike detected.",
        L"GPU is at or near maximum utilization.",
        (L"GPU at " + std::to_wstring((int)current.gpuPct) + L"% utilization."),
        10
      });
    }
  }
}

} // namespace monix
