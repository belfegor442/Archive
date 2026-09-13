#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "../Snapshot.hpp"
#include "CausalChain.hpp"

namespace monix::telemetry {

enum class CorrelationType {
  Positive,         // Both increase together
  Negative,         // One increases, other decreases
  Lagging,          // A leads B by some time
  Leading,          // A follows B by some time
  Threshold,        // A exceeds threshold triggers B
  Inverse           // A decreases when B increases
};

struct CorrelationRule {
  const char* fromField = nullptr;
  const char* toField = nullptr;
  CorrelationType type = CorrelationType::Positive;
  double threshold = 0.0;
  int lagSamples = 0;
  double minConfidence = 0.5;
};

struct CorrelationResult {
  const char* fromField = nullptr;
  const char* toField = nullptr;
  CorrelationType type = CorrelationType::Positive;
  double correlation = 0.0;
  double confidence = 0.0;
  int sampleCount = 0;
  bool active = false;
};

struct CorrelationEvent {
  const char* fromField = nullptr;
  const char* toField = nullptr;
  const char* category = nullptr;
  const char* description = nullptr;
  CorrelationType type = CorrelationType::Positive;
  double correlation = 0.0;
  std::uint64_t timestampNs = 0;
  std::string detail;
};

class CorrelationEngine {
public:
  CorrelationEngine() = default;

  void AddRule(CorrelationRule rule) {
    rules_.push_back(std::move(rule));
    CorrelationResult result;
    result.fromField = rules_.back().fromField;
    result.toField = rules_.back().toField;
    result.type = rules_.back().type;
    results_.push_back(result);
  }

  void AddDefaultRules() {
    AddRule({"cpuPct", "cpuCoreTempC", CorrelationType::Positive, 5.0, 2, 0.6});
    AddRule({"cpuPct", "contextSwitchesPerSec", CorrelationType::Positive, 10.0, 1, 0.5});
    AddRule({"cpuPct", "processorQueueLength", CorrelationType::Positive, 8.0, 1, 0.5});
    AddRule({"ramUsedBytes", "commitPressurePct", CorrelationType::Positive, 50000000, 1, 0.5});
    AddRule({"gpuPct", "gpuTempC", CorrelationType::Positive, 10.0, 2, 0.6});
    AddRule({"gpuPct", "frameTimeMs", CorrelationType::Positive, 15.0, 1, 0.5});
    AddRule({"diskReadBytesPerSec", "diskQueueLength", CorrelationType::Positive, 1000000, 1, 0.4});
    AddRule({"diskWriteBytesPerSec", "diskQueueLength", CorrelationType::Positive, 1000000, 1, 0.4});
    AddRule({"contextSwitchesPerSec", "interruptsPerSec", CorrelationType::Positive, 2000, 1, 0.4});
    AddRule({"cpuCoreTempC", "cpuThrottling", CorrelationType::Threshold, 80.0, 2, 0.7});
    AddRule({"outboundConnections", "netDownBytesPerSec", CorrelationType::Positive, 50, 1, 0.4});
    AddRule({"inboundConnections", "netUpBytesPerSec", CorrelationType::Positive, 50, 1, 0.4});
    AddRule({"processCount", "handleCount", CorrelationType::Positive, 10, 1, 0.4});
    AddRule({"threadCount", "contextSwitchesPerSec", CorrelationType::Positive, 50, 1, 0.4});
    AddRule({"diskReadIops", "diskReadLatencyMs", CorrelationType::Positive, 1000, 2, 0.5});
    AddRule({"diskWriteIops", "diskWriteLatencyMs", CorrelationType::Positive, 1000, 2, 0.5});
  }

  void Observe(const Snapshot& current, const Snapshot* previous, std::uint64_t timestampNs) {
    for (std::size_t i = 0; i < rules_.size(); ++i) {
      auto& rule = rules_[i];
      auto& result = results_[i];

      const double fromCur = GetField(current, rule.fromField);
      const double toCur = GetField(current, rule.toField);

      if (fromCur < 0 || toCur < 0) continue;

      if (previous) {
        const double fromPrev = GetField(*previous, rule.fromField);
        const double toPrev = GetField(*previous, rule.toField);

        if (fromPrev >= 0 && toPrev >= 0) {
          const double fromDelta = fromCur - fromPrev;
          const double toDelta = toCur - toPrev;

          bool correlated = false;
          switch (rule.type) {
            case CorrelationType::Positive:
              correlated = (fromDelta > rule.threshold && toDelta > 0) ||
                           (fromDelta < -rule.threshold && toDelta < 0);
              break;
            case CorrelationType::Negative:
              correlated = (fromDelta > rule.threshold && toDelta < 0) ||
                           (fromDelta < -rule.threshold && toDelta > 0);
              break;
            case CorrelationType::Threshold:
              correlated = (fromCur > rule.threshold && toCur > 0);
              break;
            case CorrelationType::Inverse:
              correlated = (fromDelta > rule.threshold && toDelta < -rule.threshold);
              break;
            default:
              break;
          }

          result.sampleCount++;
          if (correlated) {
            result.correlation = result.correlation * 0.8 + 0.2;
          } else {
            result.correlation = result.correlation * 0.9;
          }

          if (result.sampleCount >= rule.lagSamples) {
            result.confidence = result.correlation;
            if (result.confidence >= rule.minConfidence && !result.active) {
              result.active = true;
              EmitEvent(rule, result, timestampNs);
            } else if (result.confidence < rule.minConfidence * 0.5 && result.active) {
              result.active = false;
            }
          }
        }
      }
    }
  }

  const std::vector<CorrelationResult>& Results() const { return results_; }
  const std::vector<CorrelationEvent>& Events() const { return events_; }
  void ClearEvents() { events_.clear(); }

  void SetCausalChain(CausalChain* chain) { causalChain_ = chain; }

private:
  static double GetField(const Snapshot& s, const char* name);

  void EmitEvent(const CorrelationRule& rule, const CorrelationResult& result, std::uint64_t tsNs) {
    CorrelationEvent evt;
    evt.fromField = rule.fromField;
    evt.toField = rule.toField;
    evt.type = rule.type;
    evt.correlation = result.correlation;
    evt.timestampNs = tsNs;
    events_.push_back(std::move(evt));
  }

  std::vector<CorrelationRule> rules_;
  std::vector<CorrelationResult> results_;
  std::vector<CorrelationEvent> events_;
  CausalChain* causalChain_ = nullptr;
};

}