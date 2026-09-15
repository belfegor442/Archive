#pragma once

#include <cstdint>

namespace monix {

struct StorageState {
  uint64_t readBytesPerSec = 0;
  uint64_t writeBytesPerSec = 0;
  uint64_t totalBytes = 0;
  uint64_t freeBytes = 0;
  double pctUsed = 0.0;
  double queueLength = 0.0;
  double readLatencyMs = 0.0;
  double writeLatencyMs = 0.0;
  uint64_t readIops = 0;
  uint64_t writeIops = 0;
  int smartHealthOk = -1;
  double nvmeTempC = 0.0;
  int nvmeTempValid = 0;
  double tempC = 0.0;
  bool tempEstimated = false;
};

}
