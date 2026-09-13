#pragma once

#include <algorithm>
#include <cstdint>
#include <vector>

namespace monix::scram {

struct RiskInput {
  const char* category = nullptr;
  int baseScore = 0;
  double weight = 1.0;
  bool confirmed = false;
};

struct RiskOutput {
  int rawScore = 0;
  int smoothedScore = 0;
  double emaAlpha = 0.3;
  int maxScore = 100;
  int budgetUsed = 0;
  int budgetTotal = 60;
};

class RiskScorer {
public:
  RiskScorer() = default;

  void SetSmoothing(double alpha) { alpha_ = alpha; }
  void SetBudget(int budget) { budget_ = budget; }
  void SetMaxScore(int max) { maxScore_ = max; }

  int Evaluate(const std::vector<RiskInput>& inputs) {
    rawScore_ = 0;
    budgetUsed_ = 0;

    std::vector<RiskInput> sorted = inputs;
    std::sort(sorted.begin(), sorted.end(),
      [](const RiskInput& a, const RiskInput& b) {
        return a.baseScore > b.baseScore;
      });

    for (std::size_t i = 0; i < sorted.size(); ++i) {
      const auto& inp = sorted[i];
      int effectiveScore = inp.baseScore;

      if (i >= 8) {
        effectiveScore = inp.baseScore / (1 + static_cast<int>(i - 7));
      }

      effectiveScore = static_cast<int>(effectiveScore * inp.weight);

      if (effectiveScore <= 0) continue;

      if (budgetUsed_ + effectiveScore > budget_) {
        effectiveScore = std::max(0, budget_ - budgetUsed_);
      }

      budgetUsed_ += effectiveScore;
      rawScore_ += effectiveScore;
    }

    rawScore_ = std::clamp(rawScore_, 0, maxScore_);

    smoothedScore_ = static_cast<int>(
      smoothedScore_ * (1.0 - alpha_) + rawScore_ * alpha_);

    return smoothedScore_;
  }

  int RawScore() const { return rawScore_; }
  int SmoothedScore() const { return smoothedScore_; }
  int BudgetUsed() const { return budgetUsed_; }
  int BudgetTotal() const { return budget_; }

  void Reset() {
    rawScore_ = 0;
    smoothedScore_ = 0;
    budgetUsed_ = 0;
  }

private:
  double alpha_ = 0.3;
  int budget_ = 60;
  int maxScore_ = 100;
  int rawScore_ = 0;
  int smoothedScore_ = 0;
  int budgetUsed_ = 0;
};

}