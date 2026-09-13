#include "CpuPressureRule.hpp"

#include "../../telemetry/Snapshot.hpp"

namespace monix {

void CpuPressureRule::Evaluate(const Snapshot& current,
                               const Snapshot* /*previous*/,
                               std::vector<ScramFinding>& findings) {
  if (current.processorQueueLength < 0) return;
  if (current.cpuLogicalCpus > 0 && current.processorQueueLength > static_cast<int>(current.cpuLogicalCpus) * 64) return;

  const double cpuPct = current.cpuPct;
  const int queueLen = current.processorQueueLength;

  bool currentlyActive = cpuActive_;
  if (!currentlyActive) {
    if (cpuPct >= 85.0 || queueLen >= 4) {
      ++cpuActiveSamples_;
      if (cpuActiveSamples_ >= 3) {
        currentlyActive = true;
        cpuActive_ = true;
        cpuInactiveSamples_ = 0;
      }
    } else {
      cpuActiveSamples_ = 0;
    }
  } else {
    if (cpuPct < 80.0 && queueLen < 3) {
      ++cpuInactiveSamples_;
      if (cpuInactiveSamples_ >= 3) {
        cpuActive_ = false;
        cpuActiveSamples_ = 0;
        return;
      }
    } else {
      cpuInactiveSamples_ = 0;
    }
  }

  if (!currentlyActive) return;

  int riskDelta = 10;
  if (cpuPct >= 95.0) {
    riskDelta = 26;
  } else if (cpuPct >= 85.0) {
    riskDelta = 18;
  } else if (queueLen >= 6) {
    riskDelta = 20;
  } else if (queueLen >= 4) {
    riskDelta = 12;
  }

  const std::wstring diagnostic =
    L"cpu=" + std::to_wstring(static_cast<int>(cpuPct)) + L"%" +
    L" queue=" + std::to_wstring(queueLen) +
    L" threads=" + std::to_wstring(current.threadCount) +
    L" handles=" + std::to_wstring(current.handleCount);

  findings.push_back({
    L"CPU scheduling pressure detected.",
    L"Foreground activity is saturating execution slices or queue depth.",
    diagnostic,
    riskDelta
  });
}

} // namespace monix
