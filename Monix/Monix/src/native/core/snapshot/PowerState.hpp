#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <cstdint>

namespace monix {

struct PowerState {
  int acLineStatus = -1;
  int batteryFlag = 128;
  int batteryLifePercent = -1;
  long long batteryLifeTimeSec = -1;
  int batteryChargeRate = 0;
  int batteryChargeState = 0;
  int batteryChargePercent = -1;
  int batteryWearLevel = -1;
  int batteryCycleCount = -1;
  int batteryTemperature = -1;
  int powerPlanIndex = -1;
  GUID powerPlanGuid = {};
  int powerSaverActive = 0;
  int highPerfActive = 0;
  int balancedActive = 0;
  int modernStandbyActive = -1;
  int sleepStateActive = 0;
  int hibernateActive = 0;
  int cpuPackagePowerCap = -1;
  int systemPowerCap = -1;
  int idlePowerDrawHigh = 0;
};

}
