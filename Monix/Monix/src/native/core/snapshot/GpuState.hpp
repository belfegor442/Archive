#pragma once

#include <cstdint>
#include <string>

namespace monix {

struct GpuState {
  double pct = 0.0;
  int pctValid = 0;
  double tempC = 0.0;
  bool tempEstimated = false;
  std::wstring model;
  std::wstring driverVersion;
  uint64_t vramTotalBytes = 0;
  uint64_t vramUsedBytes = 0;
  double powerWatts = 0.0;
  int fanRpm = 0;
  double frameTimeMs = 0.0;
  int tdrLevel = 3;
};

}
