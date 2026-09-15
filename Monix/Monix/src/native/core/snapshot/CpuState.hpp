#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <cstdint>
#include <set>

namespace monix {

struct CpuState {
  double pct = 0.0;
  double kernelTimePct = 0.0;
  double userTimePct = 0.0;
  char vendor[13] = {};
  char brand[49] = {};
  uint32_t family = 0;
  uint32_t model = 0;
  uint32_t stepping = 0;
  uint32_t cores = 0;
  uint32_t logicalCpus = 0;
  uint32_t htEnabled = 0;
  uint64_t tscPerSec = 0;
  uint32_t microcodeRev = 0;
  uint32_t featuresEdx = 0;
  uint32_t featuresEcx = 0;
  uint32_t extFeatures = 0;
  double baseClockMhz = 0.0;
  double estimatedFrequencyMhz = 0.0;
  double kernelUserRatio = 0.0;
  uint64_t tscDelta = 0;
  double ipcEstimate = 0.0;
  int processorQueueLength = 0;
  int contextSwitchesPerSec = 0;
  int systemCallsPerSec = 0;
  int interruptsPerSec = 0;
  int processorCount = 0;
  uint64_t totalContextSwitches = 0;
  uint64_t totalInterruptCount = 0;
  uint64_t totalDpcCount = 0;
  uint64_t dpcTimePerSec = 0;
  uint64_t totalIsrCount = 0;
  uint64_t isrTimePerSec = 0;
  int suspendedThreadCount = 0;
  int readyThreadCount = 0;
  int waitingThreadCount = 0;
  int realtimeThreadCount = 0;
  int highPriorityThreadCount = 0;
  std::set<int> processorAffinities;
  int threadCountDelta = 0;
  int threadCreationDelta = 0;
  int threadTerminationDelta = 0;
};

}
