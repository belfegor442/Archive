#include "CpuIdentityRule.hpp"

#include "../../telemetry/Snapshot.hpp"
#include "../../core/TextUtils.hpp"

namespace monix {

void CpuIdentityRule::Evaluate(const Snapshot& current,
                               const Snapshot* previous,
                               std::vector<ScramFinding>& findings) {
  if (previous) {
    const auto& prev = *previous;

    if (current.cpuFamily != prev.cpuFamily || current.cpuModel != prev.cpuModel || current.cpuStepping != prev.cpuStepping) {
      findings.push_back({
        L"CPU model/stepping change detected.",
        L"Processor identification changed between samples.",
        (L"CPUID change: family " + std::to_wstring(prev.cpuFamily) + L"->" + std::to_wstring(current.cpuFamily)
          + L" model " + std::to_wstring(prev.cpuModel) + L"->" + std::to_wstring(current.cpuModel)
          + L" step " + std::to_wstring(prev.cpuStepping) + L"->" + std::to_wstring(current.cpuStepping)),
        30
      });
    }

    if (current.cpuCores != prev.cpuCores || current.cpuLogicalCpus != prev.cpuLogicalCpus) {
      findings.push_back({
        L"Core/thread count mismatch.",
        L"CPU topology changed unexpectedly.",
        (L"Cores: " + std::to_wstring(prev.cpuCores) + L"->" + std::to_wstring(current.cpuCores)
          + L" | Logical: " + std::to_wstring(prev.cpuLogicalCpus) + L"->" + std::to_wstring(current.cpuLogicalCpus)),
        30
      });
    }

    if (current.cpuHtEnabled != prev.cpuHtEnabled) {
      findings.push_back({
        L"Hyper-Threading / SMT state changed.",
        L"SMT configuration altered.",
        (L"HTT: " + std::wstring(prev.cpuHtEnabled ? L"enabled" : L"disabled")
          + L" -> " + std::wstring(current.cpuHtEnabled ? L"enabled" : L"disabled")),
        25
      });
    }

    if (current.cpuMicrocodeRev != prev.cpuMicrocodeRev) {
      findings.push_back({
        L"Microcode version change detected.",
        L"CPU microcode was updated between samples.",
        (L"Microcode: " + std::to_wstring(prev.cpuMicrocodeRev) + L" -> " + std::to_wstring(current.cpuMicrocodeRev)),
        25
      });
    }

    if (current.cpuBaseClockMhz > 0 && current.estimatedFrequencyMhz > 0) {
      const double drift = std::abs(current.estimatedFrequencyMhz - current.cpuBaseClockMhz) / current.cpuBaseClockMhz * 100.0;
      if (drift > 5.0) {
        findings.push_back({
          (L"Base clock drift detected at " + std::to_wstring((int)drift) + L"%"),
          L"TSC timing relationship has shifted.",
          (L"Estimated clock: " + std::to_wstring((int)current.estimatedFrequencyMhz)
            + L" MHz vs base " + std::to_wstring((int)current.cpuBaseClockMhz) + L" MHz"),
          10
        });
      }
    }

    if (current.cpuPct >= 95.0 && current.cpuCoreTempC >= 80.0
        && (prev.cpuPct < 95.0 || prev.cpuCoreTempC < 80.0)) {
      findings.push_back({
        L"Thermal throttling detected.",
        L"CPU temperature high under sustained load.",
        (L"CPU at " + std::to_wstring((int)current.cpuPct) + L"% with temp "
          + std::to_wstring((int)current.cpuCoreTempC) + L"C."),
        14
      });
    }

    if (current.cpuPct >= 90.0 && current.kernelUserRatio > 2.0
        && (prev.cpuPct < 90.0 || prev.kernelUserRatio <= 2.0)) {
      findings.push_back({
        L"Power limit throttling (PL1) suspected.",
        L"Kernel time disproportionately high under sustained load.",
        (L"Kernel/user ratio: " + std::to_wstring((int)(current.kernelUserRatio * 100)) + L"% at "
          + std::to_wstring((int)current.cpuPct) + L"% CPU."),
        10
      });
    }

    if (current.cpuPct >= 95.0 && current.kernelUserRatio > 3.0
        && (prev.cpuPct < 95.0 || prev.kernelUserRatio <= 3.0)) {
      findings.push_back({
        L"Power limit throttling (PL2) suspected.",
        L"Extreme kernel time ratio indicates power budget exceeded.",
        (L"PL2 indicator: kernel ratio " + std::to_wstring((int)(current.kernelUserRatio * 100))
          + L"% at peak load."),
        12
      });
    }

    if (current.cpuPct >= 85.0 && current.kernelUserRatio > 1.5
        && (prev.cpuPct < 85.0 || prev.kernelUserRatio <= 1.5)) {
      findings.push_back({
        L"Current limit throttling suspected.",
        L"Elevated kernel time at high utilization indicates current limit throttling.",
        L"Current limit throttling indicator: elevated kernel time at high utilization.",
        6
      });
    }

    if (current.cpuCoreTempC > 85.0 && prev.cpuCoreTempC <= 85.0) {
      findings.push_back({
        L"Package power excursion detected.",
        L"CPU temperature exceeded safe operating envelope.",
        (L"Package temp " + std::to_wstring((int)current.cpuCoreTempC) + L"C at "
          + std::to_wstring((int)current.cpuPct) + L"% load."),
        14
      });
    }

    if (current.ipcEstimate > 0 && prev.ipcEstimate > 0) {
      const double ipcDelta = current.ipcEstimate - prev.ipcEstimate;
      if (std::abs(ipcDelta) > 0.15) {
        findings.push_back({
          L"Instruction retirement rate anomaly.",
          L"IPC shifted significantly between samples.",
          (L"Instruction retirement rate anomaly: IPC shift "
            + std::to_wstring((int)(ipcDelta * 100)) + L"%"),
          6
        });
      }
    }

    if (current.cpuFeaturesEdx != prev.cpuFeaturesEdx || current.cpuExtFeatures != prev.cpuExtFeatures) {
      findings.push_back({
        L"CPU feature flags changed.",
        L"Processor feature set altered between samples.",
        (L"Features EDX: 0x" + std::to_wstring(current.cpuFeaturesEdx)
          + L" ext: 0x" + std::to_wstring(current.cpuExtFeatures)),
        20
      });
    }

    const double prevKuRatio = prev.kernelUserRatio;
    if (current.kernelUserRatio > 2.5 && prevKuRatio < 1.5) {
      findings.push_back({
        L"Kernel/user time imbalance detected.",
        L"Kernel time disproportionately increased.",
        (L"Kernel/user ratio surged from " + std::to_wstring((int)(prevKuRatio * 100))
          + L"% to " + std::to_wstring((int)(current.kernelUserRatio * 100)) + L"%"),
        10
      });
    }

    bool prevVmx = (prev.cpuFeaturesEcx & (1 << 5)) != 0;
    bool currVmx = (current.cpuFeaturesEcx & (1 << 5)) != 0;
    if (prevVmx != currVmx) {
      findings.push_back({
        L"Virtualization instruction activity changed.",
        L"VMX/SVM feature flag toggled between samples.",
        L"Virtualization instruction activity changed.",
        4
      });
    }
  }

  if (current.cpuPct >= 95.0 && (!previous || previous->cpuPct < 95.0)) {
    findings.push_back({
      (L"CPU saturated at " + std::to_wstring((int)current.cpuPct) + L"%."),
      L"CPU utilization at or near maximum.",
      (L"CPU utilization saturated at " + std::to_wstring((int)current.cpuPct) + L"%."),
      8
    });
  }

  if (current.cpuPct >= 70.0 && current.interruptsPerSec > 500
      && (!previous || previous->cpuPct < 70.0 || previous->interruptsPerSec <= 500)) {
    findings.push_back({
      L"Interrupt load on CPU detected.",
      L"High interrupt rate at elevated utilization.",
      (L"Interrupt load on CPU: " + std::to_wstring(current.interruptsPerSec) + L"/s at "
        + std::to_wstring((int)current.cpuPct) + L"% utilization."),
      6
    });
  }
}

} // namespace monix
