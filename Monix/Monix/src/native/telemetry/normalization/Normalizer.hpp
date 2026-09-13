#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "../Snapshot.hpp"

namespace monix::telemetry {

enum class NormalizeResult {
  Ok,
  Clamped,
  Converted,
  Estimated,
  Unavailable
};

struct NormalizeEvent {
  const char* field = nullptr;
  NormalizeResult result = NormalizeResult::Ok;
  double originalValue = 0.0;
  double normalizedValue = 0.0;
  const char* reason = nullptr;
};

class Normalizer {
public:
  Normalizer() = default;

  void Normalize(Snapshot& snap) {
    NormalizeCpuPct(snap);
    NormalizeRam(snap);
    NormalizeGpu(snap);
    NormalizeDisk(snap);
    NormalizeNetwork(snap);
    NormalizeThermal(snap);
    NormalizePower(snap);
    NormalizeTime(snap);
    NormalizeIops(snap);
  }

  const std::vector<NormalizeEvent>& Events() const { return events_; }
  void ClearEvents() { events_.clear(); }

private:
  void Emit(const char* field, NormalizeResult result, double orig, double norm, const char* reason = nullptr) {
    events_.push_back({field, result, orig, norm, reason});
  }

  void NormalizeCpuPct(Snapshot& s) {
    const double orig = s.cpuPct;
    if (s.cpuPct < 0.0) {
      s.cpuPct = 0.0;
      Emit("cpuPct", NormalizeResult::Clamped, orig, 0.0, "negative");
    } else if (s.cpuPct > 100.0) {
      s.cpuPct = 100.0;
      Emit("cpuPct", NormalizeResult::Clamped, orig, 100.0, "over100");
    }
    s.kernelUserRatio = (s.cpuPct > 0.0 && s.userTimePct > 0.0)
      ? s.kernelTimePct / s.userTimePct
      : 0.0;
  }

  void NormalizeRam(Snapshot& s) {
    if (s.ramTotalBytes > 0 && s.ramUsedBytes > s.ramTotalBytes) {
      const auto orig = s.ramUsedBytes;
      s.ramUsedBytes = s.ramTotalBytes;
      Emit("ramUsedBytes", NormalizeResult::Clamped, static_cast<double>(orig),
        static_cast<double>(s.ramTotalBytes), "exceedsTotal");
    }
    if (s.ramTotalBytes > 0 && s.ramAvailBytes > s.ramTotalBytes) {
      s.ramAvailBytes = s.ramTotalBytes;
      Emit("ramAvailBytes", NormalizeResult::Clamped, 0.0,
        static_cast<double>(s.ramTotalBytes), "exceedsTotal");
    }
    if (s.commitLimitBytes > 0 && s.commitUsedBytes > s.commitLimitBytes) {
      s.commitUsedBytes = s.commitLimitBytes;
      Emit("commitUsedBytes", NormalizeResult::Clamped, 0.0,
        static_cast<double>(s.commitLimitBytes), "exceedsLimit");
    }
  }

  void NormalizeGpu(Snapshot& s) {
    if (s.gpuPctValid == 1) {
      if (s.gpuPct < 0.0) {
        s.gpuPct = 0.0;
        Emit("gpuPct", NormalizeResult::Clamped, 0.0, 0.0, "negative");
      } else if (s.gpuPct > 100.0) {
        s.gpuPct = 100.0;
        Emit("gpuPct", NormalizeResult::Clamped, 100.0, 100.0, "over100");
      }
    }
    if (s.gpuVramUsedBytes > 0 && s.gpuVramTotalBytes > 0 &&
        s.gpuVramUsedBytes > s.gpuVramTotalBytes) {
      s.gpuVramUsedBytes = s.gpuVramTotalBytes;
      Emit("gpuVramUsedBytes", NormalizeResult::Clamped, 0.0,
        static_cast<double>(s.gpuVramTotalBytes), "exceedsTotal");
    }
  }

  void NormalizeDisk(Snapshot& s) {
    if (s.diskTotalBytes > 0 && s.diskFreeBytes > s.diskTotalBytes) {
      s.diskFreeBytes = s.diskTotalBytes;
      Emit("diskFreeBytes", NormalizeResult::Clamped, 0.0,
        static_cast<double>(s.diskTotalBytes), "exceedsTotal");
    }
    if (s.diskTotalBytes > 0) {
      s.diskPctUsed = (static_cast<double>(s.diskTotalBytes - s.diskFreeBytes) /
        static_cast<double>(s.diskTotalBytes)) * 100.0;
    }
    if (s.pageFileTotalBytes > 0 && s.pageFileUsedBytes > s.pageFileTotalBytes) {
      s.pageFileUsedBytes = s.pageFileTotalBytes;
      Emit("pageFileUsedBytes", NormalizeResult::Clamped, 0.0,
        static_cast<double>(s.pageFileTotalBytes), "exceedsTotal");
    }
    if (s.pageFileTotalBytes > 0) {
      s.pagefilePctUsed = (static_cast<double>(s.pageFileUsedBytes) /
        static_cast<double>(s.pageFileTotalBytes)) * 100.0;
    }
  }

  void NormalizeNetwork(Snapshot& s) {
    if (s.inboundConnections < 0) s.inboundConnections = 0;
    if (s.outboundConnections < 0) s.outboundConnections = 0;
    if (s.latencyMs < 0 && s.latencyMs != -1) s.latencyMs = -1;
    if (s.dnsResolutionMs < 0 && s.dnsResolutionMs != -1) s.dnsResolutionMs = -1;
    if (s.pingRttMs < 0 && s.pingRttMs != -1) s.pingRttMs = -1;
  }

  void NormalizeThermal(Snapshot& s) {
    if (s.cpuCoreTempC < 0.0 && s.cpuCoreTempC != 0.0) {
      Emit("cpuCoreTempC", NormalizeResult::Clamped, s.cpuCoreTempC, 0.0, "negative");
      s.cpuCoreTempC = 0.0;
    }
    if (s.gpuTempC < 0.0 && s.gpuTempC != 0.0) {
      Emit("gpuTempC", NormalizeResult::Clamped, s.gpuTempC, 0.0, "negative");
      s.gpuTempC = 0.0;
    }
    if (s.nvmeTempC < 0.0 && s.nvmeTempC != 0.0) {
      Emit("nvmeTempC", NormalizeResult::Clamped, s.nvmeTempC, 0.0, "negative");
      s.nvmeTempC = 0.0;
    }
    if (s.motherboardTempC < 0.0) s.motherboardTempC = 0.0;
    if (s.vrmTempC < 0.0) s.vrmTempC = 0.0;
  }

  void NormalizePower(Snapshot& s) {
    if (s.batteryLifePercent < -1) s.batteryLifePercent = -1;
    if (s.batteryLifePercent > 100) s.batteryLifePercent = 100;
    if (s.batteryChargePercent < -1) s.batteryChargePercent = -1;
    if (s.batteryChargePercent > 100) s.batteryChargePercent = 100;
    if (s.batteryWearLevel < -1) s.batteryWearLevel = -1;
    if (s.batteryWearLevel > 100) s.batteryWearLevel = 100;
    if (s.batteryTemperature < -1) s.batteryTemperature = -1;
  }

  void NormalizeTime(Snapshot& s) {
    if (s.uptimeMs == 0 && s.uptimeSeconds > 0) {
      s.uptimeMs = s.uptimeSeconds * 1000ULL;
      Emit("uptimeMs", NormalizeResult::Converted,
        static_cast<double>(s.uptimeSeconds), static_cast<double>(s.uptimeMs), "secondsToMs");
    }
  }

  void NormalizeIops(Snapshot& s) {
    if (s.diskReadIops < 0) s.diskReadIops = 0;
    if (s.diskWriteIops < 0) s.diskWriteIops = 0;
    if (s.diskReadBytesPerSec < 0) s.diskReadBytesPerSec = 0;
    if (s.diskWriteBytesPerSec < 0) s.diskWriteBytesPerSec = 0;
    if (s.netUpBytesPerSec < 0) s.netUpBytesPerSec = 0;
    if (s.netDownBytesPerSec < 0) s.netDownBytesPerSec = 0;
  }

  std::vector<NormalizeEvent> events_;
};

}