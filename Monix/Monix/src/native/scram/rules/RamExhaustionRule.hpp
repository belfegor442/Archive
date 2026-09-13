#pragma once

#include "../RiskRule.hpp"

namespace monix {

class RamExhaustionRule : public RiskRule {
 public:
  const wchar_t* Name() const override { return L"RamExhaustion"; }
  void Evaluate(const Snapshot& current,
                const Snapshot* previous,
                std::vector<ScramFinding>& findings) override;

 private:
  bool ramActive_ = false;
  int ramActiveSamples_ = 0;
  int ramInactiveSamples_ = 0;
};

} // namespace monix
