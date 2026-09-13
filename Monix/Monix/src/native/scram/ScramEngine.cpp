#include "ScramEngine.hpp"

#include <algorithm>
#include <functional>

namespace monix {

ScramEngine::ScramEngine() = default;

void ScramEngine::AddRule(std::unique_ptr<RiskRule> rule) {
  rules_.push_back(std::move(rule));
}

static std::size_t HashFindingKey(const std::wstring& headline, int riskDelta) {
  std::size_t h = std::hash<std::wstring>{}(headline);
  h ^= std::hash<int>{}(riskDelta) << 1;
  return h;
}

ScramResult ScramEngine::Evaluate(const Snapshot& current,
                                  const Snapshot* previous,
                                  const std::wstring& previousHeadline) {
  ScramResult result;
  result.riskScore = 0;
  ++totalEvaluations_;

  if (phase_ == ScramPhase::Uninitialized) {
    phase_ = ScramPhase::Calibrating;
    calibrationSamples_ = 0;
    result.headline = L"SCRAM calibrating baseline\u2026";
    result.insight = L"Waiting for telemetry samples before evaluating risk.";
    return result;
  }

  if (phase_ == ScramPhase::Calibrating) {
    ++calibrationSamples_;
    if (calibrationSamples_ >= kCalibrationRequired) {
      phase_ = ScramPhase::Ready;
    }
    result.headline = L"SCRAM calibrating \u2014 sample " +
      std::to_wstring(calibrationSamples_) + L"/" + std::to_wstring(kCalibrationRequired);
    result.insight = L"Collecting baseline data. No risk evaluation during calibration.";
    return result;
  }

  if (phase_ == ScramPhase::Ready) {
    phase_ = ScramPhase::Monitoring;
  }

  for (auto& [key, state] : findingState_) {
    if (state.cooldownRemaining > 0) {
      --state.cooldownRemaining;
    }
  }

  std::vector<ScramFinding> allFindings;
  for (const auto& rule : rules_) {
    rule->Evaluate(current, previous, allFindings);
  }

  std::sort(allFindings.begin(), allFindings.end(),
            [](const ScramFinding& a, const ScramFinding& b) {
              return a.riskDelta > b.riskDelta;
            });

  std::vector<ScramFinding> filteredFindings;
  for (const auto& f : allFindings) {
    std::size_t key = HashFindingKey(f.headline, f.riskDelta);
    auto& state = findingState_[key];

    state.lastRiskDelta = f.riskDelta;
    state.presentInLastTick = true;

    if (state.cooldownRemaining > 0) {
      continue;
    }

    ++state.debounceCount;

    if (state.debounceCount >= kDebounceRequired) {
      if (state.wasEmitted && state.lastEmittedKey == key) {
        state.debounceCount = 0;
        continue;
      }
      state.wasEmitted = true;
      state.lastEmittedKey = key;
      state.cooldownRemaining = kCooldownSamples;
      state.debounceCount = 0;
      filteredFindings.push_back(f);
    }
  }

  for (auto& [key, state] : findingState_) {
    if (!state.presentInLastTick && state.debounceCount > 0) {
      state.debounceCount = 0;
    }
    if (!state.presentInLastTick && state.wasEmitted) {
      state.wasEmitted = false;
      state.lastEmittedKey = 0;
    }
    state.presentInLastTick = false;
  }

  int riskBudget = 60;
  int riskAccumulated = 0;
  for (int i = 0; i < static_cast<int>(filteredFindings.size()); ++i) {
    const auto& f = filteredFindings[i];
    int effectiveDelta = f.riskDelta;
    if (i >= 8) {
      effectiveDelta = f.riskDelta / (1 + (i - 7));
    }
    if (effectiveDelta <= 0) continue;
    if (riskAccumulated + effectiveDelta > riskBudget) {
      effectiveDelta = std::max(0, riskBudget - riskAccumulated);
    }
    riskAccumulated += effectiveDelta;
    result.riskScore += effectiveDelta;
    if (!filteredFindings[i].headline.empty() && result.headline.empty()) {
      result.headline = filteredFindings[i].headline;
      result.insight = filteredFindings[i].insight;
    }
    if (!f.diagnostic.empty()) {
      result.diagnostics.push_back(f.diagnostic);
    }
  }

  result.riskScore = std::clamp(result.riskScore, 0, 100);

  return result;
}

} // namespace monix
