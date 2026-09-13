#include "RamExhaustionRule.hpp"

#include "../../telemetry/Snapshot.hpp"
#include "../../core/TextUtils.hpp"

namespace monix {

void RamExhaustionRule::Evaluate(const Snapshot& current,
                                 const Snapshot* /*previous*/,
                                 std::vector<ScramFinding>& findings) {
  if (current.ramTotalBytes == 0) return;

  const double ramPct = (static_cast<double>(current.ramUsedBytes) /
                         static_cast<double>(current.ramTotalBytes)) * 100.0;

  bool currentlyActive = ramActive_;
  if (!currentlyActive) {
    if (ramPct >= 82.0) {
      ++ramActiveSamples_;
      if (ramActiveSamples_ >= 3) {
        currentlyActive = true;
        ramActive_ = true;
        ramInactiveSamples_ = 0;
      }
    } else {
      ramActiveSamples_ = 0;
    }
  } else {
    if (ramPct < 78.0) {
      ++ramInactiveSamples_;
      if (ramInactiveSamples_ >= 3) {
        ramActive_ = false;
        ramActiveSamples_ = 0;
        return;
      }
    } else {
      ramInactiveSamples_ = 0;
    }
  }

  if (!currentlyActive) return;

  const int ramUsedMB = static_cast<int>(current.ramUsedBytes / (1024ull * 1024ull));
  const int ramTotalMB = static_cast<int>(current.ramTotalBytes / (1024ull * 1024ull));
  const std::wstring detail = FormatPercent(ramPct) + L" (" +
                              std::to_wstring(ramUsedMB) + L"/" +
                              std::to_wstring(ramTotalMB) + L" MB)";

  if (ramPct >= 95.0) {
    findings.push_back({
      L"RAM exhausted at " + detail,
      L"System at imminent risk of OOM kills and stall.",
      L"RAM exhaustion zone. Immediate intervention required.",
      24
    });
  } else if (ramPct >= 90.0) {
    findings.push_back({
      L"RAM critical at " + detail,
      L"Resident sets exceeding safe operating limits.",
      L"RAM in critical zone. Working sets must be reduced.",
      20
    });
  } else if (ramPct >= 85.0) {
    findings.push_back({
      L"RAM rising at " + detail,
      L"Resident sets are growing faster than reclaim and cache release.",
      L"RAM escalation past 85%. Reclaim rate insufficient.",
      18
    });
  } else if (ramPct >= 82.0) {
    findings.push_back({
      L"RAM elevated at " + detail,
      L"Resident sets are growing faster than reclaim and cache release.",
      L"RAM occupancy crossed 82% threshold.",
      15
    });
  }
}

} // namespace monix
