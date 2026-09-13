#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "../Snapshot.hpp"
#include "../contract/SensorState.hpp"

namespace monix::telemetry {

enum class ValidationLevel {
  Valid,
  Suspicious,
  Invalid,
  Missing
};

struct ValidationIssue {
  const char* field = nullptr;
  ValidationLevel level = ValidationLevel::Valid;
  const char* message = nullptr;
  double value = 0.0;
};

class Validator {
public:
  Validator() = default;

  void Validate(const Snapshot& snap, const Snapshot* prev = nullptr) {
    issues_.clear();
    ValidateCpu(snap, prev);
    ValidateRam(snap, prev);
    ValidateGpu(snap);
    ValidateDisk(snap);
    ValidateNetwork(snap);
    ValidateThermal(snap);
    ValidatePower(snap);
    ValidateProcesses(snap);
    ValidateScheduler(snap, prev);
    ValidateSecurity(snap);
    ValidateAudio(snap);
  }

  bool IsValid() const { return issues_.empty(); }
  bool HasSuspicious() const {
    for (const auto& i : issues_)
      if (i.level == ValidationLevel::Suspicious) return true;
    return false;
  }
  bool HasInvalid() const {
    for (const auto& i : issues_)
      if (i.level == ValidationLevel::Invalid) return true;
    return false;
  }

  const std::vector<ValidationIssue>& Issues() const { return issues_; }
  void ClearIssues() { issues_.clear(); }

private:
  void Issue(const char* field, ValidationLevel level, const char* msg, double val = 0.0) {
    issues_.push_back({field, level, msg, val});
  }

  void ValidateCpu(const Snapshot& s, const Snapshot* prev) {
    if (s.cpuPct < 0.0 || s.cpuPct > 100.0) {
      Issue("cpuPct", ValidationLevel::Invalid, "outOfRange", s.cpuPct);
    }
    if (s.kernelTimePct < 0.0 || s.kernelTimePct > 100.0) {
      Issue("kernelTimePct", ValidationLevel::Invalid, "outOfRange", s.kernelTimePct);
    }
    if (s.userTimePct < 0.0 || s.userTimePct > 100.0) {
      Issue("userTimePct", ValidationLevel::Invalid, "outOfRange", s.userTimePct);
    }
    if (s.cpuCores == 0) {
      Issue("cpuCores", ValidationLevel::Suspicious, "zeroCores");
    }
    if (s.cpuLogicalCpus == 0) {
      Issue("cpuLogicalCpus", ValidationLevel::Suspicious, "zeroLogical");
    }
    if (s.cpuTscPerSec == 0) {
      Issue("cpuTscPerSec", ValidationLevel::Suspicious, "zeroTsc");
    }
    if (prev) {
      const double cpuDelta = s.cpuPct - prev->cpuPct;
      if (cpuDelta > 50.0) {
        Issue("cpuPct", ValidationLevel::Suspicious, "largeSpike", cpuDelta);
      }
    }
  }

  void ValidateRam(const Snapshot& s, const Snapshot* prev) {
    if (s.ramTotalBytes == 0) {
      Issue("ramTotalBytes", ValidationLevel::Missing, "noRamInfo");
    }
    if (s.ramTotalBytes > 0 && s.ramUsedBytes > s.ramTotalBytes) {
      Issue("ramUsedBytes", ValidationLevel::Invalid, "exceedsTotal", s.ramUsedBytes);
    }
    if (s.commitLimitBytes > 0 && s.commitUsedBytes > s.commitLimitBytes) {
      Issue("commitUsedBytes", ValidationLevel::Suspicious, "exceedsCommitLimit");
    }
    if (prev && s.ramTotalBytes > 0) {
      const double usedPct = (static_cast<double>(s.ramUsedBytes) /
        static_cast<double>(s.ramTotalBytes)) * 100.0;
      if (usedPct > 95.0) {
        Issue("ramUsedBytes", ValidationLevel::Suspicious, "highUsage", usedPct);
      }
    }
  }

  void ValidateGpu(const Snapshot& s) {
    if (s.gpuPctValid == 1) {
      if (s.gpuPct < 0.0 || s.gpuPct > 100.0) {
        Issue("gpuPct", ValidationLevel::Invalid, "outOfRange", s.gpuPct);
      }
    }
    if (s.gpuVramTotalBytes > 0 && s.gpuVramUsedBytes > s.gpuVramTotalBytes) {
      Issue("gpuVramUsedBytes", ValidationLevel::Invalid, "exceedsTotal");
    }
    if (s.frameTimeMs < 0.0) {
      Issue("frameTimeMs", ValidationLevel::Invalid, "negative");
    }
    if (s.frameTimeMs > 1000.0) {
      Issue("frameTimeMs", ValidationLevel::Suspicious, "extremelyHigh", s.frameTimeMs);
    }
  }

  void ValidateDisk(const Snapshot& s) {
    if (s.diskTotalBytes > 0 && s.diskFreeBytes > s.diskTotalBytes) {
      Issue("diskFreeBytes", ValidationLevel::Invalid, "exceedsTotal");
    }
    if (s.diskQueueLength < 0.0) {
      Issue("diskQueueLength", ValidationLevel::Invalid, "negative", s.diskQueueLength);
    }
    if (s.diskReadLatencyMs < 0.0) {
      Issue("diskReadLatencyMs", ValidationLevel::Invalid, "negative");
    }
    if (s.diskWriteLatencyMs < 0.0) {
      Issue("diskWriteLatencyMs", ValidationLevel::Invalid, "negative");
    }
    if (s.smartHealthOk == 0) {
      Issue("smartHealthOk", ValidationLevel::Suspicious, "driveFailing");
    }
  }

  void ValidateNetwork(const Snapshot& s) {
    if (s.inboundConnections < 0) {
      Issue("inboundConnections", ValidationLevel::Invalid, "negative");
    }
    if (s.outboundConnections < 0) {
      Issue("outboundConnections", ValidationLevel::Invalid, "negative");
    }
    if (s.latencyMs >= 0 && s.latencyMs > 5000) {
      Issue("latencyMs", ValidationLevel::Suspicious, "extremelyHigh", s.latencyMs);
    }
    if (s.dnsResolutionOk == 0 && s.dnsResolutionMs > 0) {
      Issue("dnsResolutionMs", ValidationLevel::Suspicious, "failedButPositive");
    }
    if (s.tcpResets > 1000) {
      Issue("tcpResets", ValidationLevel::Suspicious, "highCount", s.tcpResets);
    }
  }

  void ValidateThermal(const Snapshot& s) {
    if (s.cpuCoreTempC > 115.0) {
      Issue("cpuCoreTempC", ValidationLevel::Suspicious, "extremelyHigh", s.cpuCoreTempC);
    }
    if (s.gpuTempC > 115.0) {
      Issue("gpuTempC", ValidationLevel::Suspicious, "extremelyHigh", s.gpuTempC);
    }
    if (s.nvmeTempC > 80.0) {
      Issue("nvmeTempC", ValidationLevel::Suspicious, "extremelyHigh", s.nvmeTempC);
    }
    if (s.cpuThrottleTempC > 0.0 && s.cpuCoreTempC > s.cpuThrottleTempC) {
      Issue("cpuCoreTempC", ValidationLevel::Suspicious, "aboveThrottleTemp", s.cpuCoreTempC);
    }
  }

  void ValidatePower(const Snapshot& s) {
    if (s.batteryLifePercent < -1 || s.batteryLifePercent > 100) {
      Issue("batteryLifePercent", ValidationLevel::Invalid, "outOfRange", s.batteryLifePercent);
    }
    if (s.batteryChargePercent < -1 || s.batteryChargePercent > 100) {
      Issue("batteryChargePercent", ValidationLevel::Invalid, "outOfRange", s.batteryChargePercent);
    }
    if (s.batteryWearLevel < -1 || s.batteryWearLevel > 100) {
      Issue("batteryWearLevel", ValidationLevel::Invalid, "outOfRange", s.batteryWearLevel);
    }
    if (s.batteryTemperature > 60) {
      Issue("batteryTemperature", ValidationLevel::Suspicious, "overheating", s.batteryTemperature);
    }
  }

  void ValidateProcesses(const Snapshot& s) {
    if (s.processCount < 0) {
      Issue("processCount", ValidationLevel::Invalid, "negative");
    }
    if (s.threadCount < 0) {
      Issue("threadCount", ValidationLevel::Invalid, "negative");
    }
    if (s.handleCount < 0) {
      Issue("handleCount", ValidationLevel::Invalid, "negative");
    }
    for (const auto& p : s.processes) {
      if (p.pid <= 0) continue;
      if (p.cpuPct < 0.0 || p.cpuPct > 100.0 * s.cpuLogicalCpus) {
        Issue("processCpuPct", ValidationLevel::Suspicious, "outOfRange", p.cpuPct);
      }
      if (p.ramBytes > s.ramTotalBytes && s.ramTotalBytes > 0) {
        Issue("processRamBytes", ValidationLevel::Suspicious, "exceedsSystemTotal", p.ramBytes);
      }
    }
  }

  void ValidateScheduler(const Snapshot& s, const Snapshot* prev) {
    if (s.processorQueueLength < 0) {
      Issue("processorQueueLength", ValidationLevel::Invalid, "negative");
    }
    if (s.contextSwitchesPerSec < 0) {
      Issue("contextSwitchesPerSec", ValidationLevel::Invalid, "negative");
    }
    if (s.interruptsPerSec < 0) {
      Issue("interruptsPerSec", ValidationLevel::Invalid, "negative");
    }
    if (prev) {
      if (s.threadCount > 0 && prev->threadCount > 0) {
        const int threadDelta = s.threadCount - prev->threadCount;
        if (threadDelta > 500 || threadDelta < -500) {
          Issue("threadCount", ValidationLevel::Suspicious, "extremeDelta", threadDelta);
        }
      }
    }
  }

  void ValidateSecurity(const Snapshot& s) {
    if (s.selfHashComputed == -1) {
      Issue("selfHashComputed", ValidationLevel::Missing, "hashNotComputed");
    }
    if (s.selfSignatureValid == -1) {
      Issue("selfSignatureValid", ValidationLevel::Missing, "signatureNotChecked");
    }
  }

  void ValidateAudio(const Snapshot& s) {
    if (s.audioSampleRate < 0) {
      Issue("audioSampleRate", ValidationLevel::Invalid, "negative");
    }
    if (s.audioLatencyMs < 0) {
      Issue("audioLatencyMs", ValidationLevel::Invalid, "negative");
    }
    if (s.audioLatencyMs > 500) {
      Issue("audioLatencyMs", ValidationLevel::Suspicious, "extremelyHigh", s.audioLatencyMs);
    }
  }

  std::vector<ValidationIssue> issues_;
};

}