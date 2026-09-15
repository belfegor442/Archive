#pragma once

#include <cstdint>
#include <vector>

namespace monix {

struct ThermalState {
  double cpuCoreTempC = 0.0;
  double cpuCoreTempMax = 0.0;
  double motherboardTempC = 0.0;
  double vrmTempC = 0.0;
  double ambientTempC = 0.0;
  double cpuThrottleTempC = 0.0;
  int cpuThrottling = 0;
  int fanCount = 0;
  std::vector<int> fanSpeeds;
  int pumpSpeed = 0;
  int pumpPresent = 0;
  int thermalSensorCount = 0;
  int thermalSensorFailures = 0;
  double thermalTrendC = 0.0;
  int thermalRecoveryCount = 0;
  double thermalHeatSoakIndex = 0.0;
  double coolingCurveSlope = 0.0;
};

}
