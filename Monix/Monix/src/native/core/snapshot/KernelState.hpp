#pragma once

#include <cstdint>
#include <set>
#include <string>

namespace monix {

struct KernelState {
  unsigned long long systemTime100ns = 0;
  unsigned long long uptimeMs = 0;
  unsigned long driverCount = 0;
  std::set<std::wstring> driverNames;
  unsigned long long totalHandles = 0;
  unsigned long long totalObjects = 0;
  unsigned long long totalSyscalls = 0;
  unsigned long long ntStatusErrors = 0;
  unsigned long long registryOps = 0;
  unsigned long sessionCount = 0;
  int currentSessionId = 0;
  unsigned long long ioReadBytesDelta = 0;
  unsigned long long ioWriteBytesDelta = 0;
  unsigned long long pnpDeviceCount = 0;
  int timeChangeDetected = 0;
  int processCount = 0;
  int threadCount = 0;
  int handleCount = 0;
};

}
