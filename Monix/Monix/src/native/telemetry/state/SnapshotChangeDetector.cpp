#include "SnapshotChangeDetector.hpp"

#include "../Snapshot.hpp"

namespace monix::telemetry {

double SnapshotChangeDetector::GetField(const Snapshot& s, const char* name) {
  const std::string_view sv(name);

  if (sv == "cpuPct") return s.cpuPct;
  if (sv == "ramUsedBytes") return static_cast<double>(s.ramUsedBytes);
  if (sv == "gpuPct") return s.gpuPctValid == 1 ? s.gpuPct : -1.0;
  if (sv == "diskReadBytesPerSec") return static_cast<double>(s.diskReadBytesPerSec);
  if (sv == "diskWriteBytesPerSec") return static_cast<double>(s.diskWriteBytesPerSec);
  if (sv == "netUpBytesPerSec") return static_cast<double>(s.netUpBytesPerSec);
  if (sv == "netDownBytesPerSec") return static_cast<double>(s.netDownBytesPerSec);
  if (sv == "contextSwitchesPerSec") return s.contextSwitchesPerSec;
  if (sv == "interruptsPerSec") return s.interruptsPerSec;
  if (sv == "processorQueueLength") return s.processorQueueLength;
  if (sv == "processCount") return s.processCount;
  if (sv == "threadCount") return s.threadCount;
  if (sv == "handleCount") return s.handleCount;
  if (sv == "pageFileUsedBytes") return static_cast<double>(s.pageFileUsedBytes);
  if (sv == "diskQueueLength") return s.diskQueueLength;
  if (sv == "diskReadLatencyMs") return s.diskReadLatencyMs;
  if (sv == "diskWriteLatencyMs") return s.diskWriteLatencyMs;
  if (sv == "frameTimeMs") return s.frameTimeMs;
  if (sv == "cpuCoreTempC") return s.cpuCoreTempC;
  if (sv == "motherboardTempC") return s.motherboardTempC;
  if (sv == "cpuTscDelta") return static_cast<double>(s.cpuTscDelta);
  if (sv == "pageFaultsDelta") return static_cast<double>(s.pageFaultsDelta);
  if (sv == "hardPageFaultsDelta") return static_cast<double>(s.hardPageFaultsDelta);
  if (sv == "ioReadBytesDelta") return static_cast<double>(s.ioReadBytesDelta);
  if (sv == "ioWriteBytesDelta") return static_cast<double>(s.ioWriteBytesDelta);
  if (sv == "tcpRetransmits") return static_cast<double>(s.tcpRetransmits);
  if (sv == "tcpResets") return static_cast<double>(s.tcpResets);
  if (sv == "diskReadIops") return static_cast<double>(s.diskReadIops);
  if (sv == "diskWriteIops") return static_cast<double>(s.diskWriteIops);

  return -1.0;
}

}