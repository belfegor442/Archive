#pragma once

#include "../RiskRule.hpp"

namespace monix {

class GpuVramRule : public RiskRule {
 public:
  const wchar_t* Name() const override { return L"GpuVram"; }
  void Evaluate(const Snapshot& current,
                const Snapshot* previous,
                std::vector<ScramFinding>& findings) override;
 private:
  bool prevVramSaturated_ = false;
};

} // namespace monix
