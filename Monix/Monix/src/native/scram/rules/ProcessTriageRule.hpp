#pragma once

#include "../RiskRule.hpp"
#include <unordered_map>

namespace monix {

class ProcessTriageRule : public RiskRule {
 public:
  const wchar_t* Name() const override { return L"ProcessTriage"; }
  void Evaluate(const Snapshot& current,
                const Snapshot* previous,
                std::vector<ScramFinding>& findings) override;
 private:
  struct ReportedState {
    double lastReportedCpuPct = 0.0;
    std::uint64_t lastReportedRamBytes = 0;
    int reportCount = 0;
  };
  std::unordered_map<int, ReportedState> reported_;
};

} // namespace monix
