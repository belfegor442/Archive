#include "CorrelationEngine.hpp"

#include "../Snapshot.hpp"

namespace monix::telemetry {

double CorrelationEngine::GetField(const Snapshot& s, const char* name) {
  const std::string_view sv(name);

  if (sv == "cpuPct") return s.cpuPct;
  if (sv == "kernelTimePct") return s.kernelTimePct;
  if (sv == "userTimePct") return s.userTimePct;
  if (sv == "ramUsedBytes") return static_cast<double>(s.ramUsedBytes);
  if (sv == "ramTotalBytes") return static_cast<double>(s.ramTotalBytes);
  if (sv == "gpuPct") return s.gpuPctValid == 1 ? s.gpuPct : -1.0;
  if (sv == "gpuTempC") return s.gpuTempC;
  if (sv == "gpuVramUsedBytes") return static_cast<double>(s.gpuVramUsedBytes);
  if (sv == "gpuVramTotalBytes") return static_cast<double>(s.gpuVramTotalBytes);
  if (sv == "diskReadBytesPerSec") return static_cast<double>(s.diskReadBytesPerSec);
  if (sv == "diskWriteBytesPerSec") return static_cast<double>(s.diskWriteBytesPerSec);
  if (sv == "diskReadIops") return static_cast<double>(s.diskReadIops);
  if (sv == "diskWriteIops") return static_cast<double>(s.diskWriteIops);
  if (sv == "diskQueueLength") return s.diskQueueLength;
  if (sv == "diskReadLatencyMs") return s.diskReadLatencyMs;
  if (sv == "diskWriteLatencyMs") return s.diskWriteLatencyMs;
  if (sv == "diskTotalBytes") return static_cast<double>(s.diskTotalBytes);
  if (sv == "diskFreeBytes") return static_cast<double>(s.diskFreeBytes);
  if (sv == "netUpBytesPerSec") return static_cast<double>(s.netUpBytesPerSec);
  if (sv == "netDownBytesPerSec") return static_cast<double>(s.netDownBytesPerSec);
  if (sv == "inboundConnections") return s.inboundConnections;
  if (sv == "outboundConnections") return s.outboundConnections;
  if (sv == "latencyMs") return s.latencyMs >= 0 ? s.latencyMs : -1.0;
  if (sv == "dnsResolutionMs") return s.dnsResolutionMs >= 0 ? s.dnsResolutionMs : -1.0;
  if (sv == "contextSwitchesPerSec") return s.contextSwitchesPerSec;
  if (sv == "interruptsPerSec") return s.interruptsPerSec;
  if (sv == "processorQueueLength") return s.processorQueueLength;
  if (sv == "processCount") return s.processCount;
  if (sv == "threadCount") return s.threadCount;
  if (sv == "handleCount") return s.handleCount;
  if (sv == "cpuCoreTempC") return s.cpuCoreTempC;
  if (sv == "cpuThrottleTempC") return s.cpuThrottleTempC;
  if (sv == "cpuThrottling") return s.cpuThrottling;
  if (sv == "motherboardTempC") return s.motherboardTempC;
  if (sv == "vrmTempC") return s.vrmTempC;
  if (sv == "nvmeTempC") return s.nvmeTempValid == 1 ? s.nvmeTempC : -1.0;
  if (sv == "frameTimeMs") return s.frameTimeMs;
  if (sv == "commitPressurePct") return s.commitPressurePct;
  if (sv == "pagefilePctUsed") return s.pagefilePctUsed;
  if (sv == "pageFileUsedBytes") return static_cast<double>(s.pageFileUsedBytes);
  if (sv == "pageFileTotalBytes") return static_cast<double>(s.pageFileTotalBytes);
  if (sv == "batteryChargePercent") return s.batteryChargePercent >= 0 ? s.batteryChargePercent : -1.0;
  if (sv == "batteryLifePercent") return s.batteryLifePercent >= 0 ? s.batteryLifePercent : -1.0;
  if (sv == "batteryTemperature") return s.batteryTemperature >= 0 ? s.batteryTemperature : -1.0;
  if (sv == "audioLatencyMs") return s.audioLatencyMs >= 0 ? s.audioLatencyMs : -1.0;
  if (sv == "tcpResets") return static_cast<double>(s.tcpResets);
  if (sv == "tcpRetransmits") return static_cast<double>(s.tcpRetransmits);

  return -1.0;
}

}