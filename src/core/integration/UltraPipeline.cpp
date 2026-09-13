#include "UltraPipeline.hpp"

#include <algorithm>
#include <chrono>

namespace monix::integration {

const char* PipelineStageName(PipelineStage stage) {
  switch (stage) {
    case PipelineStage::Collection:        return "Collection";
    case PipelineStage::Normalization:     return "Normalization";
    case PipelineStage::EventBuilding:     return "EventBuilding";
    case PipelineStage::Validation:        return "Validation";
    case PipelineStage::Quarantine:        return "Quarantine";
    case PipelineStage::NoiseControl:      return "NoiseControl";
    case PipelineStage::Correlation:       return "Correlation";
    case PipelineStage::RuleEvaluation:    return "RuleEvaluation";
    case PipelineStage::ContextEnrichment: return "ContextEnrichment";
    case PipelineStage::Storage:           return "Storage";
    case PipelineStage::LiveStream:        return "LiveStream";
    case PipelineStage::Analysis:          return "Analysis";
  }
  return "Unknown";
}

const char* PipelineStateName(PipelineState state) {
  switch (state) {
    case PipelineState::Idle:     return "Idle";
    case PipelineState::Running:  return "Running";
    case PipelineState::Paused:   return "Paused";
    case PipelineState::Error:    return "Error";
    case PipelineState::Shutdown: return "Shutdown";
  }
  return "Unknown";
}

bool PipelineEvent::isValid() const {
  return !event_id.empty() && !event_type.empty();
}

std::string PipelineResult::summary() const {
  return "accepted=" + std::string(accepted ? "yes" : "no") +
    " quarantined=" + std::string(quarantined ? "yes" : "no") +
    " filtered=" + std::string(noisy_filtered ? "yes" : "no") +
    " error=" + error;
}

std::string PipelineMetrics::summary() const {
  return "received=" + std::to_string(events_received) +
    " valid=" + std::to_string(events_valid) +
    " stored=" + std::to_string(events_stored) +
    " quarantined=" + std::to_string(events_quarantined) +
    " filtered=" + std::to_string(events_filtered) +
    " correlated=" + std::to_string(events_correlated) +
    " analysis=" + std::to_string(analysis_events) +
    " errors=" + std::to_string(errors);
}

UltraPipeline::UltraPipeline() {
  for (int i = 0; i <= static_cast<int>(PipelineStage::Analysis); i++) {
    stage_enabled_[static_cast<PipelineStage>(i)] = true;
  }
}

UltraPipeline::~UltraPipeline() {}

void UltraPipeline::setValidator(EventValidator validator) {
  std::lock_guard<std::mutex> lock(mu_);
  validator_ = std::move(validator);
}

void UltraPipeline::setFilter(EventFilter filter) {
  std::lock_guard<std::mutex> lock(mu_);
  filter_ = std::move(filter);
}

void UltraPipeline::setStore(EventStore store) {
  std::lock_guard<std::mutex> lock(mu_);
  store_ = std::move(store);
}

void UltraPipeline::setEventCallback(EventCallback callback) {
  std::lock_guard<std::mutex> lock(mu_);
  event_callback_ = std::move(callback);
}

void UltraPipeline::setAnalysisCallback(AnalysisCallback callback) {
  std::lock_guard<std::mutex> lock(mu_);
  analysis_callback_ = std::move(callback);
}

PipelineResult UltraPipeline::processEvent(const PipelineEvent& event) {
  std::lock_guard<std::mutex> lock(mu_);
  PipelineResult result;
  total_processed_++;
  metrics_.events_received++;

  if (state_ == PipelineState::Paused || state_ == PipelineState::Shutdown) {
    result.error = "Pipeline not accepting events";
    return result;
  }

  state_ = PipelineState::Running;

  if (!event.isValid()) {
    result.error = "Invalid event";
    metrics_.errors++;
    return result;
  }

  PipelineEvent normalized = normalize(event);

  if (stage_enabled_[PipelineStage::Validation]) {
    if (validator_ && !validator_(normalized)) {
      result.quarantined = true;
      metrics_.events_quarantined++;
      result.failed_at = PipelineStage::Validation;
      result.error = "Validation failed";
      return result;
    }
  }

  if (stage_enabled_[PipelineStage::NoiseControl]) {
    if (filter_ && !filter_(normalized)) {
      result.noisy_filtered = true;
      metrics_.events_filtered++;
      return result;
    }
  }

  if (stage_enabled_[PipelineStage::Correlation]) {
    if (!normalized.metadata.empty()) {
      result.correlated = true;
      metrics_.events_correlated++;
    }
  }

  if (stage_enabled_[PipelineStage::Storage]) {
    if (store_ && !store_(normalized)) {
      result.error = "Storage failed";
      metrics_.errors++;
      return result;
    }
    metrics_.events_stored++;
  }

  if (event_callback_) {
    event_callback_(normalized);
  }

  result.accepted = true;
  metrics_.events_valid++;
  return result;
}

PipelineState UltraPipeline::state() const {
  std::lock_guard<std::mutex> lock(mu_);
  return state_;
}

PipelineMetrics UltraPipeline::metrics() const {
  std::lock_guard<std::mutex> lock(mu_);
  return metrics_;
}

void UltraPipeline::pause() {
  std::lock_guard<std::mutex> lock(mu_);
  state_ = PipelineState::Paused;
}

void UltraPipeline::resume() {
  std::lock_guard<std::mutex> lock(mu_);
  state_ = PipelineState::Idle;
}

void UltraPipeline::shutdown() {
  std::lock_guard<std::mutex> lock(mu_);
  state_ = PipelineState::Shutdown;
}

void UltraPipeline::enableStage(PipelineStage stage, bool enabled) {
  std::lock_guard<std::mutex> lock(mu_);
  stage_enabled_[stage] = enabled;
}

bool UltraPipeline::isStageEnabled(PipelineStage stage) const {
  std::lock_guard<std::mutex> lock(mu_);
  auto it = stage_enabled_.find(stage);
  return it != stage_enabled_.end() && it->second;
}

std::size_t UltraPipeline::totalProcessed() const {
  return total_processed_;
}

PipelineEvent UltraPipeline::normalize(const PipelineEvent& event) const {
  PipelineEvent normalized = event;
  if (normalized.timestamp_ms == 0) {
    normalized.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count();
  }
  if (normalized.severity.empty()) {
    normalized.severity = "Info";
  }
  return normalized;
}

bool UltraPipeline::validate(const PipelineEvent& event) const {
  if (validator_) return validator_(event);
  return event.isValid();
}

bool UltraPipeline::applyNoiseControl(const PipelineEvent& event) const {
  if (filter_) return filter_(event);
  return true;
}

bool UltraPipeline::store(const PipelineEvent& event) const {
  if (store_) return store_(event);
  return true;
}

}  // namespace monix::integration
