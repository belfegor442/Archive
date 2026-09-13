#pragma once

#include "../RiskRule.hpp"

namespace monix {

class HardwareSensorsRule : public RiskRule {
 public:
  const wchar_t* Name() const override { return L"HardwareSensors"; }
  void Evaluate(const Snapshot& current,
                const Snapshot* previous,
                std::vector<ScramFinding>& findings) override;

 private:
  int prevSensorPollFailures_ = 0;
};

} // namespace monix
