#define NOMINMAX
#include "ThermalCoolingRule.hpp"

#include "../../telemetry/Snapshot.hpp"

#include <algorithm>
#include <cmath>

namespace monix {

void ThermalCoolingRule::Evaluate(const Snapshot& current,
                                  const Snapshot* previous,
                                  std::vector<ScramFinding>& findings) {
  if (!previous) {
    prevCpuCoreTempC_ = current.cpuCoreTempC;
    prevVrmTempC_ = current.vrmTempC;
    prevMotherboardTempC_ = current.motherboardTempC;
    prevFanSpeeds_ = current.fanSpeeds;
    prevFanCount_ = current.fanCount;
    prevPumpSpeed_ = current.pumpSpeed;
    prevCpuThrottling_ = current.cpuThrottling;
    prevThermalSensorFailures_ = current.thermalSensorFailures;
    prevCpuCoreTempMax_ = current.cpuCoreTempMax;
    prevAmbientTempC_ = current.ambientTempC;
    return;
  }
  const Snapshot& prev = *previous;

  // 1. CPU package temperature rise
  if (current.cpuCoreTempC > prev.cpuCoreTempC + 10.0 && current.cpuCoreTempC > 50.0) {
    findings.push_back({
      L"CPU temperature rising.",
      L"CPU package temperature increased by more than 10C above 50C.",
      L"CPU temp: " + std::to_wstring((int)prev.cpuCoreTempC) + L"C -> " + std::to_wstring((int)current.cpuCoreTempC) + L"C.",
      12
    });
  }

  // 2. CPU core temperature rise
  if (current.cpuCoreTempC > prevCpuCoreTempC_ + 8.0 && current.cpuCoreTempC > 55.0) {
    findings.push_back({
      L"CPU core temperature rising.",
      L"Average CPU core temperature increased by more than 8C.",
      L"Core temp: " + std::to_wstring((int)prevCpuCoreTempC_) + L"C -> " + std::to_wstring((int)current.cpuCoreTempC) + L"C.",
      12
    });
  }

  // 3. GPU temperature rise
  if (current.gpuTempC > prev.gpuTempC + 15.0 && current.gpuTempC > 60.0 && !current.gpuTempEstimated) {
    findings.push_back({
      L"GPU temperature rising.",
      L"GPU temperature increased by more than 15C above 60C.",
      L"GPU temp: " + std::to_wstring((int)prev.gpuTempC) + L"C -> " + std::to_wstring((int)current.gpuTempC) + L"C.",
      12
    });
  }

  // 4. VRM temperature rise
  if (current.vrmTempC > prevVrmTempC_ + 8.0 && current.vrmTempC > 60.0) {
    findings.push_back({
      L"VRM temperature rising.",
      L"Voltage regulator module temperature increased significantly.",
      L"VRM temp: " + std::to_wstring((int)prevVrmTempC_) + L"C -> " + std::to_wstring((int)current.vrmTempC) + L"C.",
      10
    });
  }

  // 5. Motherboard temperature rise
  if (current.motherboardTempC > prevMotherboardTempC_ + 5.0 && current.motherboardTempC > 40.0) {
    findings.push_back({
      L"Motherboard temperature rising.",
      L"Motherboard sensor temperature increased by more than 5C.",
      L"MB temp: " + std::to_wstring((int)prevMotherboardTempC_) + L"C -> " + std::to_wstring((int)current.motherboardTempC) + L"C.",
      10
    });
  }

  // 6. SSD temperature rise
  if (current.storageTempC > prev.storageTempC + 8.0 && current.storageTempC > 45.0) {
    findings.push_back({
      L"SSD temperature rising.",
      L"Storage drive temperature increased by more than 8C.",
      L"SSD temp: " + std::to_wstring((int)prev.storageTempC) + L"C -> " + std::to_wstring((int)current.storageTempC) + L"C.",
      10
    });
  }

  // 7. Fan speed surge
  if (current.fanSpeeds.size() == prevFanSpeeds_.size() && !current.fanSpeeds.empty()) {
    for (size_t i = 0; i < current.fanSpeeds.size() && i < prevFanSpeeds_.size(); ++i) {
      if (current.fanSpeeds[i] > prevFanSpeeds_[i] * 1.5 && current.fanSpeeds[i] > 1000) {
        findings.push_back({
          L"Fan speed surge detected.",
          L"Fan RPM increased by over 50%, indicating active cooling response.",
          L"Fan " + std::to_wstring(i) + L": " + std::to_wstring(prevFanSpeeds_[i]) + L" -> " + std::to_wstring(current.fanSpeeds[i]) + L" RPM.",
          6
        });
      }
    }
  }

  // 8. Fan speed drop
  if (current.fanSpeeds.size() == prevFanSpeeds_.size() && !current.fanSpeeds.empty()) {
    for (size_t i = 0; i < current.fanSpeeds.size() && i < prevFanSpeeds_.size(); ++i) {
      if (prevFanSpeeds_[i] > 1000 && current.fanSpeeds[i] < prevFanSpeeds_[i] * 0.5) {
        findings.push_back({
          L"Fan speed drop detected.",
          L"Fan RPM dropped by over 50% under load, possibly indicating fan controller issue.",
          L"Fan " + std::to_wstring(i) + L": " + std::to_wstring(prevFanSpeeds_[i]) + L" -> " + std::to_wstring(current.fanSpeeds[i]) + L" RPM.",
          10
        });
      }
    }
  }

  // 9. Fan stall
  if (!prevFanSpeeds_.empty()) {
    for (size_t i = 0; i < prevFanSpeeds_.size(); ++i) {
      if (prevFanSpeeds_[i] > 500) {
        int curSpeed = (i < current.fanSpeeds.size()) ? current.fanSpeeds[i] : 0;
        if (curSpeed == 0) {
          findings.push_back({
            L"Fan stall detected.",
            L"A previously spinning fan has stopped while system is under load.",
            L"Fan " + std::to_wstring(i) + L" stalled: " + std::to_wstring(prevFanSpeeds_[i]) + L" -> 0 RPM.",
            16
          });
        }
      }
    }
  }

  // 10. Fan failure
  if (current.fanCount < prevFanCount_ && prevFanCount_ > 0) {
    findings.push_back({
      L"Fan failure detected.",
      L"A fan is no longer reported by the hardware monitoring subsystem.",
      L"Fan count: " + std::to_wstring(prevFanCount_) + L" -> " + std::to_wstring(current.fanCount) + L".",
      18
    });
  }

  // 11. Pump failure
  if (current.pumpPresent && current.pumpSpeed == 0 && prevPumpSpeed_ > 0) {
    findings.push_back({
      L"Water pump failure detected.",
      L"Water cooling pump has stopped while previously running.",
      L"Pump: " + std::to_wstring(prevPumpSpeed_) + L" -> 0 RPM.",
      20
    });
  }

  // 12. Cooling loop anomaly (transition-based)
  const bool coolingLoopAnomaly = current.cpuCoreTempC > 70.0 && !current.fanSpeeds.empty() && current.fanSpeeds[0] < 500;
  if (coolingLoopAnomaly && !prevCoolingLoopAnomaly_) {
    findings.push_back({
      L"Cooling loop anomaly detected.",
      L"CPU temperature is high but fan speed is low, indicating cooling system issue.",
      L"Cooling: CPU " + std::to_wstring((int)current.cpuCoreTempC) + L"C at " + std::to_wstring(current.fanSpeeds[0]) + L" RPM.",
      16
    });
  }
  prevCoolingLoopAnomaly_ = coolingLoopAnomaly;

  // 13. Thermal throttling onset
  if (current.cpuThrottling == 1 && prevCpuThrottling_ == 0) {
    findings.push_back({
      L"Thermal throttling onset.",
      L"CPU has entered thermal throttle state due to high temperature.",
      L"Throttle: CPU at " + std::to_wstring((int)current.cpuCoreTempC) + L"C, " + std::to_wstring((int)current.cpuPct) + L"% util.",
      18
    });
  }

  // 14. Thermal throttling cleared
  if (current.cpuThrottling == 0 && prevCpuThrottling_ == 1) {
    findings.push_back({
      L"Thermal throttling cleared.",
      L"CPU has exited thermal throttle state.",
      L"Throttle cleared: CPU temp now " + std::to_wstring((int)current.cpuCoreTempC) + L"C.",
      2
    });
  }

  // 15. Thermal sensor unavailable
  if (current.thermalSensorFailures > prevThermalSensorFailures_) {
    findings.push_back({
      L"Thermal sensor unavailable.",
      L"A thermal zone returned invalid or null readings.",
      L"Sensor failures: " + std::to_wstring(prevThermalSensorFailures_) + L" -> " + std::to_wstring(current.thermalSensorFailures) + L".",
      10
    });
  }

  // 16. Thermal sensor drift (transition-based)
  if (current.cpuCoreTempMax > 0.0 && current.cpuCoreTempC > 0.0) {
    const double drift = current.cpuCoreTempMax - current.cpuCoreTempC;
    const bool sensorDrift = drift > 25.0;
    if (sensorDrift && !prevSensorDrift_) {
      findings.push_back({
        L"Thermal sensor drift detected.",
        L"Large spread between hottest and average core temperature indicates uneven cooling.",
        L"Core spread: " + std::to_wstring((int)drift) + L"C (max " + std::to_wstring((int)current.cpuCoreTempMax) + L"C).",
        10
      });
    }
    prevSensorDrift_ = sensorDrift;
  }

  // 17. Hotspot temperature spike
  if (current.cpuCoreTempMax > prevCpuCoreTempMax_ + 15.0 && current.cpuCoreTempMax > 80.0) {
    findings.push_back({
      L"Hotspot temperature spike.",
      L"Hottest core temperature spiked by more than 15C, indicating localized overheating.",
      L"Hotspot: " + std::to_wstring((int)prevCpuCoreTempMax_) + L"C -> " + std::to_wstring((int)current.cpuCoreTempMax) + L"C.",
      14
    });
  }

  // 18. Ambient temperature rise
  if (current.ambientTempC > prevAmbientTempC_ + 5.0 && current.ambientTempC > 30.0) {
    findings.push_back({
      L"Ambient temperature rise.",
      L"Estimated ambient/room temperature has increased, reducing cooling efficiency.",
      L"Ambient: " + std::to_wstring((int)prevAmbientTempC_) + L"C -> " + std::to_wstring((int)current.ambientTempC) + L"C.",
      8
    });
  }

  // 19. Overtemperature warning (transition-based)
  const bool overtemp = current.cpuCoreTempC >= 85.0 || current.gpuTempC >= 85.0 || current.storageTempC >= 65.0;
  if (overtemp && !prevOvertemp_) {
    findings.push_back({
      L"Overtemperature warning.",
      L"Temperature has exceeded the safe operating envelope.",
      L"Temp: CPU " + std::to_wstring((int)current.cpuCoreTempC) + L"C, GPU " + std::to_wstring((int)current.gpuTempC) + L"C, SSD " + std::to_wstring((int)current.storageTempC) + L"C.",
      16
    });
  }
  prevOvertemp_ = overtemp;

  // 20. Critical temperature warning (transition-based)
  const bool criticalTemp = current.cpuCoreTempC >= 95.0 || current.gpuTempC >= 95.0;
  if (criticalTemp && !prevCriticalTemp_) {
    findings.push_back({
      L"Critical temperature warning.",
      L"Temperature at critical level. Hardware may shut down to prevent damage.",
      L"Critical temp: CPU " + std::to_wstring((int)current.cpuCoreTempC) + L"C, GPU " + std::to_wstring((int)current.gpuTempC) + L"C.",
      20
    });
  }
  prevCriticalTemp_ = criticalTemp;

  // 21. Thermal hysteresis event
  if (current.cpuCoreTempC > prev.cpuCoreTempC + 5.0 || current.cpuCoreTempC < prev.cpuCoreTempC - 5.0) {
    findings.push_back({
      L"Thermal hysteresis detected.",
      L"Temperature oscillating rapidly between samples, indicating unstable thermal state.",
      L"Hysteresis: " + std::to_wstring((int)prev.cpuCoreTempC) + L"C -> " + std::to_wstring((int)current.cpuCoreTempC) + L"C.",
      10
    });
  }

  // 22. Heat soak detection
  if (current.cpuCoreTempC > 70.0 && current.cpuPct > 80.0) {
    if (heatSoakBaseline_ == 0.0) heatSoakBaseline_ = current.cpuCoreTempC;
    if (current.cpuCoreTempC > heatSoakBaseline_ + 10.0) {
      findings.push_back({
        L"Heat soak detected.",
        L"CPU temperature has risen steadily under sustained load, indicating heat soak.",
        L"Heat soak: +10C above " + std::to_wstring((int)heatSoakBaseline_) + L"C baseline.",
        12
      });
    }
  } else {
    heatSoakBaseline_ = 0.0;
  }

  // 23. Cooling curve change
  if (current.cpuCoreTempC < prev.cpuCoreTempC - 5.0 && prev.cpuPct > 80.0 && current.cpuPct < 30.0) {
    findings.push_back({
      L"Cooling curve change.",
      L"CPU temperature dropped quickly after load reduction, indicating cooling system response.",
      L"Cooling: " + std::to_wstring((int)prev.cpuCoreTempC) + L"C -> " + std::to_wstring((int)current.cpuCoreTempC) + L"C after load drop.",
      2
    });
  }

  // 24. Thermal recovery event
  if (prev.cpuCoreTempC >= 85.0 && current.cpuCoreTempC < 70.0) {
    findings.push_back({
      L"Thermal recovery event.",
      L"System has recovered from overtemperature condition.",
      L"Recovery: " + std::to_wstring((int)prev.cpuCoreTempC) + L"C -> " + std::to_wstring((int)current.cpuCoreTempC) + L"C.",
      2
    });
  }

  // 25. Airflow obstruction indicator
  if (current.cpuCoreTempC > 70.0 && current.fanSpeeds.size() == prevFanSpeeds_.size()) {
    bool allFansMax = true;
    for (size_t i = 0; i < current.fanSpeeds.size(); ++i) {
      int maxFan = 0;
      if (i < prevFanSpeeds_.size()) maxFan = std::max(prevFanSpeeds_[i], current.fanSpeeds[i]);
      if (maxFan < 2000) allFansMax = false;
    }
    if (allFansMax && current.cpuCoreTempC > prev.cpuCoreTempC) {
      findings.push_back({
        L"Airflow obstruction suspected.",
        L"Fans at high speed but temperature still rising, indicating possible airflow blockage.",
        L"Airflow: fans at high RPM, temp " + std::to_wstring((int)current.cpuCoreTempC) + L"C rising.",
        14
      });
    }
  }

  prevCpuCoreTempC_ = current.cpuCoreTempC;
  prevVrmTempC_ = current.vrmTempC;
  prevMotherboardTempC_ = current.motherboardTempC;
  prevFanSpeeds_ = current.fanSpeeds;
  prevFanCount_ = current.fanCount;
  prevPumpSpeed_ = current.pumpSpeed;
  prevCpuThrottling_ = current.cpuThrottling;
  prevThermalSensorFailures_ = current.thermalSensorFailures;
  prevCpuCoreTempMax_ = current.cpuCoreTempMax;
  prevAmbientTempC_ = current.ambientTempC;
}

} // namespace monix
