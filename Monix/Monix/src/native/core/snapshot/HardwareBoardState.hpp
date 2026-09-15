#pragma once

#include <cstdint>
#include <string>

namespace monix {

struct HardwareBoardState {
  unsigned long smbiosHash = 0;
  unsigned long acpiHash = 0;
  std::wstring biosVersion;
  std::wstring biosManufacturer;
  int biosMajorVer = 0;
  int biosMinorVer = 0;
  double voltage12V = 0.0;
  double voltage5V = 0.0;
  double voltage33V = 0.0;
  double voltageVcore = 0.0;
  double voltageVram = 0.0;
  int tpmPresent = 0;
  int tpmReady = 0;
  int tpmVersion = 0;
  int cmosBatteryOk = 1;
  int sensorPollFailures = 0;
  int pcieErrors = 0;
  int usbResets = 0;
  int sataResets = 0;
  int thunderboltEvents = 0;
  int ecEvents = 0;
  double boardTempHotspot = 0.0;
};

}
