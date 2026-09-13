#include "ThresholdDetector.hpp"

#include "../Snapshot.hpp"

namespace monix::telemetry {

double ThresholdDetector::GetField(const Snapshot& s, const char* name) {
  const std::string_view sv(name);

  if (sv == "cpuPct") return s.cpuPct;
  if (sv == "ramUsedBytes") return static_cast<double>(s.ramUsedBytes);
  if (sv == "ramTotalBytes") return static_cast<double>(s.ramTotalBytes);
  if (sv == "gpuPct") return s.gpuPctValid == 1 ? s.gpuPct : -1.0;
  if (sv == "diskQueueLength") return s.diskQueueLength;
  if (sv == "diskReadLatencyMs") return s.diskReadLatencyMs;
  if (sv == "diskWriteLatencyMs") return s.diskWriteLatencyMs;
  if (sv == "contextSwitchesPerSec") return s.contextSwitchesPerSec;
  if (sv == "processorQueueLength") return s.processorQueueLength;
  if (sv == "pagefilePctUsed") return s.pagefilePctUsed;
  if (sv == "commitPressurePct") return s.commitPressurePct;
  if (sv == "cpuCoreTempC") return s.cpuCoreTempC;
  if (sv == "cpuThrottleTempC") return s.cpuThrottleTempC;
  if (sv == "motherboardTempC") return s.motherboardTempC;
  if (sv == "vrmTempC") return s.vrmTempC;
  if (sv == "nvmeTempC") return s.nvmeTempValid == 1 ? s.nvmeTempC : -1.0;
  if (sv == "gpuTempC") return s.gpuTempC;
  if (sv == "frameTimeMs") return s.frameTimeMs;
  if (sv == "diskPctUsed") return s.diskPctUsed;
  if (sv == "batteryChargePercent") return s.batteryChargePercent >= 0 ? s.batteryChargePercent : -1.0;
  if (sv == "latencyMs") return s.latencyMs >= 0 ? s.latencyMs : -1.0;
  if (sv == "dnsResolutionMs") return s.dnsResolutionMs >= 0 ? s.dnsResolutionMs : -1.0;
  if (sv == "processCount") return s.processCount;
  if (sv == "threadCount") return s.threadCount;
  if (sv == "handleCount") return s.handleCount;

  return -1.0;
}

}