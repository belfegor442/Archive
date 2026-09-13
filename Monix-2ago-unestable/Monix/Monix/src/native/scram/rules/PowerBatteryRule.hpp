#pragma once

#include "../RiskRule.hpp"

namespace monix {

class PowerBatteryRule : public RiskRule {
 public:
  const wchar_t* Name() const override { return L"PowerBattery"; }
  void Evaluate(const Snapshot& current,
                const Snapshot* previous,
                std::vector<ScramFinding>& findings) override;

 private:
  int prevIdlePowerDrawHigh_ = 0;
  int prevPowerSaverActive_ = 0;
  int prevHighPerfActive_ = 0;
};

} // namespace monix
