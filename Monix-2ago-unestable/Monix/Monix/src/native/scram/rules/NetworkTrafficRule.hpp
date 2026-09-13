#pragma once

#include "../RiskRule.hpp"

namespace monix {

class NetworkTrafficRule : public RiskRule {
 public:
  const wchar_t* Name() const override { return L"NetworkTraffic"; }
  void Evaluate(const Snapshot& current,
                const Snapshot* previous,
                std::vector<ScramFinding>& findings) override;
 private:
  bool prevBandwidthSaturation_ = false;
  bool prevPortScan_ = false;
  bool prevNetworkPressure_ = false;
};

} // namespace monix
