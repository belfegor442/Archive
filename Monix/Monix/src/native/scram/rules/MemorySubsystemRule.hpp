#pragma once

#include "../RiskRule.hpp"

namespace monix {

class MemorySubsystemRule : public RiskRule {
 public:
  const wchar_t* Name() const override { return L"MemorySubsystem"; }
  void Evaluate(const Snapshot& current,
                const Snapshot* previous,
                std::vector<ScramFinding>& findings) override;
};

} // namespace monix
