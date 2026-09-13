#pragma once

#include "../RiskRule.hpp"

namespace monix {

class FrameTimeRule : public RiskRule {
 public:
  const wchar_t* Name() const override { return L"FrameTime"; }
  void Evaluate(const Snapshot& current,
                const Snapshot* previous,
                std::vector<ScramFinding>& findings) override;
 private:
  bool prevRenderBacklog_ = false;
};

} // namespace monix
