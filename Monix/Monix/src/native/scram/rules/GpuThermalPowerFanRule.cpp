#include "GpuThermalPowerFanRule.hpp"

#include "../../telemetry/Snapshot.hpp"
#include "../../core/TextUtils.hpp"

namespace monix {

void GpuThermalPowerFanRule::Evaluate(const Snapshot& current,
                                      const Snapshot* previous,
                                      std::vector<ScramFinding>& findings) {
  if (!previous) return;

  if (current.gpuTempC > previous->gpuTempC + 15.0 && current.gpuTempC > 60.0) {
    findings.push_back({
      L"GPU temperature rise detected.",
      L"GPU temperature jumped significantly.",
      (L"GPU temp: " + FormatTemperature(previous->gpuTempC, previous->gpuTempEstimated) + L" -> " + FormatTemperature(current.gpuTempC, current.gpuTempEstimated)),
      10
    });
  }

  if (current.gpuPowerWatts > previous->gpuPowerWatts * 1.5 && current.gpuPowerWatts > 100.0) {
    findings.push_back({
      L"GPU power spike detected.",
      L"GPU power draw increased by over 50%.",
      (L"GPU power: " + std::to_wstring((int)previous->gpuPowerWatts) + L"W -> " + std::to_wstring((int)current.gpuPowerWatts) + L"W"),
      12
    });
  }

  if (previous->gpuFanRpm > 3000 && current.gpuFanRpm < previous->gpuFanRpm * 0.3) {
    findings.push_back({
      L"GPU fan anomaly detected.",
      L"GPU fan speed dropped significantly while under load.",
      (L"Fan: " + std::to_wstring(previous->gpuFanRpm) + L" -> " + std::to_wstring(current.gpuFanRpm) + L" RPM"),
      12
    });
  }
}

} // namespace monix
