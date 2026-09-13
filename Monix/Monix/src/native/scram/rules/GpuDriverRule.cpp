#include "GpuDriverRule.hpp"

#include "../../telemetry/Snapshot.hpp"

namespace monix {

void GpuDriverRule::Evaluate(const Snapshot& current,
                             const Snapshot* previous,
                             std::vector<ScramFinding>& findings) {
  if (!previous) return;

  if (!current.gpuModel.empty() && current.gpuModel != previous->gpuModel) {
    findings.push_back({
      (L"GPU model detection: " + current.gpuModel),
      L"GPU hardware identifier has changed.",
      (L"GPU: " + current.gpuModel),
      6
    });
  }

  if (!current.gpuDriverVersion.empty() && current.gpuDriverVersion != previous->gpuDriverVersion && !previous->gpuDriverVersion.empty()) {
    findings.push_back({
      L"GPU driver version change detected.",
      L"Display driver has been updated or rolled back.",
      (L"Driver: " + previous->gpuDriverVersion + L" -> " + current.gpuDriverVersion),
      14
    });
  }
}

} // namespace monix
