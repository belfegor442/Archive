#pragma once

#include "../RiskRule.hpp"

#include <vector>

namespace monix {

class ThermalCoolingRule : public RiskRule {
 public:
  const wchar_t* Name() const override { return L"ThermalCooling"; }
  void Evaluate(const Snapshot& current,
                const Snapshot* previous,
                std::vector<ScramFinding>& findings) override;

 private:
  double prevCpuCoreTempC_ = 0.0;
  double prevVrmTempC_ = 0.0;
  double prevMotherboardTempC_ = 0.0;
  std::vector<int> prevFanSpeeds_;
  int prevFanCount_ = 0;
  int prevPumpSpeed_ = 0;
  int prevCpuThrottling_ = 0;
  int prevThermalSensorFailures_ = 0;
  double prevCpuCoreTempMax_ = 0.0;
  double prevAmbientTempC_ = 0.0;
  double heatSoakBaseline_ = 0.0;
  bool prevCoolingLoopAnomaly_ = false;
  bool prevSensorDrift_ = false;
  bool prevOvertemp_ = false;
  bool prevCriticalTemp_ = false;
};

} // namespace monix
