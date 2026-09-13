#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "RiskRule.hpp"

namespace monix { struct Snapshot; }

namespace monix {

enum class ScramPhase : std::uint8_t {
  Uninitialized = 0,
  Calibrating = 1,
  Ready = 2,
  Monitoring = 3
};

inline const wchar_t* ScramPhaseName(ScramPhase p) {
  switch (p) {
    case ScramPhase::Uninitialized: return L"UNINITIALIZED";
    case ScramPhase::Calibrating:  return L"CALIBRATING";
    case ScramPhase::Ready:        return L"READY";
    case ScramPhase::Monitoring:   return L"MONITORING";
  }
  return L"UNKNOWN";
}

struct ScramFindingState {
  int debounceCount = 0;
  int cooldownRemaining = 0;
  bool wasEmitted = false;
  int lastRiskDelta = 0;
  bool presentInLastTick = false;
  std::size_t lastEmittedKey = 0;
};

struct ScramResult {
  std::wstring headline;
  std::wstring insight;
  std::vector<std::wstring> diagnostics;
  int riskScore = 0;
};

class ScramEngine {
 public:
  ScramEngine();

  void AddRule(std::unique_ptr<RiskRule> rule);
  std::size_t RuleCount() const { return rules_.size(); }

  ScramResult Evaluate(const Snapshot& current,
                       const Snapshot* previous,
                       const std::wstring& previousHeadline = {});

  ScramPhase phase() const { return phase_; }
  int calibrationSamples() const { return calibrationSamples_; }

  static constexpr int kDebounceRequired = 2;
  static constexpr int kCooldownSamples = 5;

 private:
  std::vector<std::unique_ptr<RiskRule>> rules_;
  ScramPhase phase_ = ScramPhase::Uninitialized;
  int calibrationSamples_ = 0;
  int totalEvaluations_ = 0;
  static constexpr int kCalibrationRequired = 5;
  std::unordered_map<std::size_t, ScramFindingState> findingState_;
};

} // namespace monix
