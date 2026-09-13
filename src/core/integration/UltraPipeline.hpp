#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace monix::integration {

enum class PipelineStage : std::uint8_t {
  Collection,
  Normalization,
  EventBuilding,
  Validation,
  Quarantine,
  NoiseControl,
  Correlation,
  RuleEvaluation,
  ContextEnrichment,
  Storage,
  LiveStream,
  Analysis
};

const char* PipelineStageName(PipelineStage stage);

enum class PipelineState : std::uint8_t {
  Idle,
  Running,
  Paused,
  Error,
  Shutdown
};

const char* PipelineStateName(PipelineState state);

struct PipelineEvent {
  std::string event_id;
  std::string event_type;
  std::string source;
  std::string severity;
  std::string payload;
  std::int64_t timestamp_ms = 0;
  std::unordered_map<std::string, std::string> metadata;

  bool isValid() const;
};

struct PipelineResult {
  bool accepted = false;
  PipelineStage failed_at = PipelineStage::Collection;
  std::string error;
  std::vector<std::string> analysis_generated;
  bool quarantined = false;
  bool noisy_filtered = false;
  bool correlated = false;

  std::string summary() const;
};

struct PipelineMetrics {
  std::size_t events_received = 0;
  std::size_t events_valid = 0;
  std::size_t events_quarantined = 0;
  std::size_t events_filtered = 0;
  std::size_t events_stored = 0;
  std::size_t events_correlated = 0;
  std::size_t analysis_events = 0;
  std::size_t errors = 0;
  std::int64_t avg_latency_us = 0;

  std::string summary() const;
};

using EventValidator = std::function<bool(const PipelineEvent&)>;
using EventFilter = std::function<bool(const PipelineEvent&)>;
using EventStore = std::function<bool(const PipelineEvent&)>;
using EventCallback = std::function<void(const PipelineEvent&)>;
using AnalysisCallback = std::function<void(const PipelineEvent&)>;

class UltraPipeline {
public:
  UltraPipeline();
  ~UltraPipeline();

  UltraPipeline(const UltraPipeline&) = delete;
  UltraPipeline& operator=(const UltraPipeline&) = delete;

  void setValidator(EventValidator validator);
  void setFilter(EventFilter filter);
  void setStore(EventStore store);
  void setEventCallback(EventCallback callback);
  void setAnalysisCallback(AnalysisCallback callback);

  PipelineResult processEvent(const PipelineEvent& event);

  PipelineState state() const;
  PipelineMetrics metrics() const;

  void pause();
  void resume();
  void shutdown();

  void enableStage(PipelineStage stage, bool enabled);
  bool isStageEnabled(PipelineStage stage) const;

  std::size_t totalProcessed() const;

private:
  PipelineEvent normalize(const PipelineEvent& event) const;
  bool validate(const PipelineEvent& event) const;
  bool applyNoiseControl(const PipelineEvent& event) const;
  bool store(const PipelineEvent& event) const;

  mutable std::mutex mu_;
  PipelineState state_ = PipelineState::Idle;
  PipelineMetrics metrics_;
  EventValidator validator_;
  EventFilter filter_;
  EventStore store_;
  EventCallback event_callback_;
  AnalysisCallback analysis_callback_;
  std::unordered_map<PipelineStage, bool> stage_enabled_;
  std::atomic<std::size_t> total_processed_{0};
};

}  // namespace monix::integration
