#pragma once

#include "../RiskRule.hpp"

namespace monix {

class SecurityIntegrityRule : public RiskRule {
 public:
  const wchar_t* Name() const override { return L"SecurityIntegrity"; }
  void Evaluate(const Snapshot& current,
                const Snapshot* previous,
                std::vector<ScramFinding>& findings) override;
};

} // namespace monix
