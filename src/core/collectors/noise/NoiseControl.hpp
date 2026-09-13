#pragma once

#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace monix::collectors::noise {

enum class NoiseMechanism : std::uint8_t {
  None = 0,
  Deduplication = 1,
  Coalescing = 2,
  RateLimiting = 4,
  Threshold = 8,
  Aggregation = 16,
  Backpressure = 32,
  Priority = 64,
  Quarantine = 128
};

inline NoiseMechanism operator|(NoiseMechanism a, NoiseMechanism b) {
  return static_cast<NoiseMechanism>(static_cast<std::uint8_t>(a) | static_cast<std::uint8_t>(b));
}

inline NoiseMechanism operator&(NoiseMechanism a, NoiseMechanism b) {
  return static_cast<NoiseMechanism>(static_cast<std::uint8_t>(a) & static_cast<std::uint8_t>(b));
}

inline bool hasMechanism(NoiseMechanism flags, NoiseMechanism m) {
  return (static_cast<std::uint8_t>(flags) & static_cast<std::uint8_t>(m)) != 0;
}

const char* NoiseMechanismName(NoiseMechanism m);

enum class NoiseVerdict : std::uint8_t {
  Pass,
  Deduplicate,
  Coalesce,
  RateLimited,
  BelowThreshold,
  Aggregated,
  Backpressured,
  Quarantined
};

const char* NoiseVerdictName(NoiseVerdict v);

struct CollectorNoisePolicy {
  std::string collector_name;
  NoiseMechanism mechanisms = NoiseMechanism::None;

  std::size_t dedup_window_ms = 1000;
  std::size_t coalesce_window_ms = 500;
  std::size_t rate_limit_max = 100;
  std::size_t rate_limit_window_ms = 1000;
  double threshold_min = 0.0;
  std::size_t aggregate_window_ms = 1000;
  std::size_t aggregate_max_batch = 50;
  std::size_t backpressure_limit = 1000;
  std::size_t quarantine_threshold = 10;

  bool isValid() const;
};

struct NoiseEvent {
  std::string event_id;
  std::string collector;
  std::string event_type;
  std::int64_t timestamp_ms = 0;
  double numeric_value = 0.0;

  bool isValid() const;
};

struct NoiseResult {
  NoiseVerdict verdict = NoiseVerdict::Pass;
  std::string reason;
  std::size_t batch_size = 0;

  bool passes() const;
  std::string summary() const;
};

class NoiseControlEngine {
public:
  NoiseControlEngine();
  ~NoiseControlEngine();

  NoiseControlEngine(const NoiseControlEngine&) = delete;
  NoiseControlEngine& operator=(const NoiseControlEngine&) = delete;

  void setPolicy(const CollectorNoisePolicy& policy);
  CollectorNoisePolicy getPolicy(const std::string& collector) const;

  NoiseResult evaluate(const NoiseEvent& event);

  std::size_t totalEvents() const;
  std::size_t totalFiltered() const;
  std::size_t totalPassed() const;

  void reset(const std::string& collector);
  void resetAll();

private:
  NoiseResult evaluateDedup(const NoiseEvent& event, const CollectorNoisePolicy& policy);
  NoiseResult evaluateRateLimit(const NoiseEvent& event, const CollectorNoisePolicy& policy);
  NoiseResult evaluateThreshold(const NoiseEvent& event, const CollectorNoisePolicy& policy);

  mutable std::mutex mu_;
  std::unordered_map<std::string, CollectorNoisePolicy> policies_;
  std::atomic<std::size_t> total_events_{0};
  std::atomic<std::size_t> total_filtered_{0};

  struct CollectorState {
    std::vector<std::pair<std::string, std::int64_t>> recent_events;
    std::size_t rate_count = 0;
    std::int64_t rate_window_start = 0;
    std::size_t quarantine_count = 0;
    bool quarantined = false;
  };

  std::unordered_map<std::string, CollectorState> states_;
};

}  // namespace monix::collectors::noise
