#include "DiskStorageRule.hpp"

#include "../../telemetry/Snapshot.hpp"

namespace monix {

void DiskStorageRule::Evaluate(const Snapshot& current,
                               const Snapshot* previous,
                               std::vector<ScramFinding>& findings) {
  if (current.smartHealthOk == 0 && (!previous || previous->smartHealthOk != 0)) {
    findings.push_back({
      L"SSD health degradation detected.",
      L"SMART reports imminent failure.",
      L"SMART health status not OK.",
      20
    });
  }

  if (current.diskPctUsed > 95.0 && (!previous || previous->diskPctUsed <= 95.0)) {
    findings.push_back({
      (L"Disk capacity critical at " + std::to_wstring((int)current.diskPctUsed) + L"%"),
      L"Storage volume nearly full.",
      (L"Disk: " + std::to_wstring(current.diskFreeBytes / (1024*1024))
        + L" MB free / " + std::to_wstring(current.diskTotalBytes / (1024*1024)) + L" MB"),
      16
    });
  } else if (current.diskPctUsed > 90.0 && (!previous || previous->diskPctUsed <= 90.0)) {
    findings.push_back({
      (L"Disk capacity elevated: " + std::to_wstring((int)current.diskPctUsed) + L"% used"),
      L"Storage volume approaching capacity.",
      (L"Disk: " + std::to_wstring(current.diskFreeBytes / (1024*1024))
        + L" MB free / " + std::to_wstring(current.diskTotalBytes / (1024*1024)) + L" MB"),
      8
    });
  }

  if (current.diskQueueLength > 4.0 && (!previous || previous->diskQueueLength <= 4.0)) {
    findings.push_back({
      (L"Disk queue depth spike: " + std::to_wstring((int)current.diskQueueLength)),
      L"I/O requests are queuing significantly.",
      (L"Queue depth " + std::to_wstring((int)current.diskQueueLength)
        + L" | read " + std::to_wstring((int)current.diskReadLatencyMs) + L"ms"
        + L" | write " + std::to_wstring((int)current.diskWriteLatencyMs) + L"ms"),
      10
    });
  } else if (current.diskQueueLength > 2.0 && (!previous || previous->diskQueueLength <= 2.0)) {
    findings.push_back({
      (L"Disk queue depth elevated: " + std::to_wstring((int)current.diskQueueLength)),
      L"Disk I/O queue depth moderately elevated.",
      (L"Queue depth: " + std::to_wstring((int)current.diskQueueLength)),
      4
    });
  }

  if (current.diskReadLatencyMs > 20.0 && (!previous || previous->diskReadLatencyMs <= 20.0)) {
    findings.push_back({
      (L"Read latency spike: " + std::to_wstring((int)current.diskReadLatencyMs) + L"ms"),
      L"Disk read operations are severely delayed.",
      (L"Read latency: " + std::to_wstring((int)current.diskReadLatencyMs) + L"ms"),
      10
    });
  } else if (current.diskReadLatencyMs > 10.0 && (!previous || previous->diskReadLatencyMs <= 10.0)) {
    findings.push_back({
      (L"Read latency elevated: " + std::to_wstring((int)current.diskReadLatencyMs) + L"ms"),
      L"Disk read latency moderately elevated.",
      (L"Read latency: " + std::to_wstring((int)current.diskReadLatencyMs) + L"ms"),
      4
    });
  }

  if (current.diskWriteLatencyMs > 20.0 && (!previous || previous->diskWriteLatencyMs <= 20.0)) {
    findings.push_back({
      (L"Write latency spike: " + std::to_wstring((int)current.diskWriteLatencyMs) + L"ms"),
      L"Disk write operations are severely delayed.",
      (L"Write latency: " + std::to_wstring((int)current.diskWriteLatencyMs) + L"ms"),
      10
    });
  } else if (current.diskWriteLatencyMs > 10.0 && (!previous || previous->diskWriteLatencyMs <= 10.0)) {
    findings.push_back({
      (L"Write latency elevated: " + std::to_wstring((int)current.diskWriteLatencyMs) + L"ms"),
      L"Disk write latency moderately elevated.",
      (L"Write latency: " + std::to_wstring((int)current.diskWriteLatencyMs) + L"ms"),
      4
    });
  }

  if (previous) {
    const uint64_t totalIops = current.diskReadIops + current.diskWriteIops;
    const uint64_t prevTotalIops = previous->previousDiskReadIops + previous->previousDiskWriteIops;

    if (totalIops > 10000 && prevTotalIops <= 10000) {
      findings.push_back({
        (L"IOPS saturation at " + std::to_wstring(totalIops)),
        L"Disk IOPS at maximum sustainable rate.",
        (L"Total IOPS: " + std::to_wstring(totalIops)),
        12
      });
    }

    if (totalIops > 5000 && current.cpuPct < 30.0 && prevTotalIops <= 5000) {
      findings.push_back({
        L"Random I/O burst detected.",
        L"High IOPS with low CPU utilization suggests random I/O pattern.",
        (L"Random I/O burst: " + std::to_wstring(totalIops) + L" IOPS"),
        6
      });
    }

    const uint64_t totalBytesIo = current.diskReadBytesPerSec + current.diskWriteBytesPerSec;
    const uint64_t prevTotalBytesIo = previous->previousDiskReadBytesPerSec + previous->previousDiskWriteBytesPerSec;
    if (totalBytesIo > 200 * 1024 * 1024 && prevTotalBytesIo <= 200 * 1024 * 1024) {
      findings.push_back({
        L"Sequential I/O burst detected.",
        L"Disk throughput exceeding 200 MB/s.",
        (L"Sequential I/O burst: " + std::to_wstring(totalBytesIo / (1024*1024)) + L" MB/s"),
        6
      });
    }

    if (current.nvmeTempValid && current.nvmeTempC > 70.0 && (!previous->nvmeTempValid || previous->nvmeTempC <= 70.0)) {
      findings.push_back({
        (L"NVMe temperature rise: " + std::to_wstring((int)current.nvmeTempC) + L"C"),
        L"NVMe drive running hot.",
        (L"NVMe temp: " + std::to_wstring((int)current.nvmeTempC) + L"C"),
        10
      });
    }

    if (current.diskQueueLength < 0.1 && previous->diskQueueLength > 1.0) {
      findings.push_back({
        L"Disk idle after heavy queue.",
        L"Possible sleep/wake cycle or I/O completion storm.",
        L"Disk idle after heavy queue: possible sleep/wake cycle",
        2
      });
    }
  }
}

} // namespace monix
