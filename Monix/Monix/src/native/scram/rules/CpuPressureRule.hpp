#pragma once

#include "../RiskRule.hpp"

namespace monix {

class CpuPressureRule : public RiskRule {
 public:
  const wchar_t* Name() const override { return L"CpuPressure"; }
  void Evaluate(const Snapshot& current,
                const Snapshot* previous,
                std::vector<ScramFinding>& findings) override;

 private:
  bool cpuActive_ = false;
  int cpuActiveSamples_ = 0;
  int cpuInactiveSamples_ = 0;
};

} // namespace monix
