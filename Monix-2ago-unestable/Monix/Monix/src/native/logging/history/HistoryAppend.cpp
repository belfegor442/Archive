#include "HistoryAppend.hpp"

#include "../../MonixApp.hpp"
#include "../../TelemetryInternal.hpp"
#include "../../telemetry/Snapshot.hpp"
#include "../../core/TextUtils.hpp"

#include <algorithm>

using namespace monix;

void MonixApp::AppendHistoryPoint(const Snapshot& snapshot) {
  monix::internal::g_phase = "APPEND:ENTER";
  if (!config_.analyticsHistoryEnabled) {
    return;
  }
  monix::internal::g_phase = "APPEND:CHECK_DONE";

  const double ramPct = snapshot.ramTotalBytes == 0 ? 0.0 :
    (static_cast<double>(snapshot.ramUsedBytes) / static_cast<double>(snapshot.ramTotalBytes)) * 100.0;
  const double netUpMb = std::min(100.0, static_cast<double>(snapshot.netUpBytesPerSec) / (1024.0 * 1024.0));
  const double netDownMb = std::min(100.0, static_cast<double>(snapshot.netDownBytesPerSec) / (1024.0 * 1024.0));

  {
    std::lock_guard<std::recursive_mutex> lock(stateMutex_);
    AppendBounded(state_.history.cpu, snapshot.cpuPct, config_.historyCapacity);
    AppendBounded(state_.history.ram, ramPct, config_.historyCapacity);
    AppendBounded(state_.history.gpu, snapshot.gpuPct, config_.historyCapacity);
    AppendBounded(state_.history.netUpload, netUpMb, config_.historyCapacity);
    AppendBounded(state_.history.net, netDownMb, config_.historyCapacity);
    AppendBounded(state_.history.latency, static_cast<double>(snapshot.latencyMs), config_.historyCapacity);
  }
}
