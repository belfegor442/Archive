#pragma once

#include <string>
#include <vector>

namespace monix { struct Snapshot; }

namespace monix {

struct ScramFinding {
  std::wstring headline;
  std::wstring insight;
  std::wstring diagnostic;
  int riskDelta = 0;
};

class RiskRule {
 public:
  virtual ~RiskRule() = default;
  virtual const wchar_t* Name() const = 0;
  virtual void Evaluate(const Snapshot& current,
                        const Snapshot* previous,
                        std::vector<ScramFinding>& findings) = 0;
};

} // namespace monix
