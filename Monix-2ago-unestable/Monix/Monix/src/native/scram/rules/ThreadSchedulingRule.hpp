#pragma once

#include "../RiskRule.hpp"

namespace monix {

class ThreadSchedulingRule : public RiskRule {
 public:
  const wchar_t* Name() const override { return L"ThreadScheduling"; }
  void Evaluate(const Snapshot& current,
                const Snapshot* previous,
                std::vector<ScramFinding>& findings) override;
};

} // namespace monix
