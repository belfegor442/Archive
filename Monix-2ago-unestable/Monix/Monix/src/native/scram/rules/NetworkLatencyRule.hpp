#pragma once

#include "../RiskRule.hpp"

#include <cmath>
#include <vector>

namespace monix {

class NetworkLatencyRule : public RiskRule {
 public:
  const wchar_t* Name() const override { return L"NetworkLatency"; }
  void Evaluate(const Snapshot& current,
                const Snapshot* previous,
                std::vector<ScramFinding>& findings) override;

 private:
  std::vector<int> pingRttSamples_;
  double pingJitterStddev_ = 0.0;
};

} // namespace monix
