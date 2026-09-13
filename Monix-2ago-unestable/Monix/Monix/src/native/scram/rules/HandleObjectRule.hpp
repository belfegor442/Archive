#pragma once

#include "../RiskRule.hpp"

namespace monix {

class HandleObjectRule : public RiskRule {
 public:
  const wchar_t* Name() const override { return L"HandleObject"; }
  void Evaluate(const Snapshot& current,
                const Snapshot* previous,
                std::vector<ScramFinding>& findings) override;
};

} // namespace monix
