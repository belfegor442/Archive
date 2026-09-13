#include "EventStormProtection.hpp"

#include <algorithm>
#include <chrono>

namespace monix::collectors::storm {

const char* StormPriorityName(StormPriority p) {
  switch (p) {
    case StormPriority::Critical:   return "Critical";
    case StormPriority::High:       return "High";
    case StormPriority::Normal:     return "Normal";
    case StormPriority::Low:        return "Low";
    case StormPriority::Background: return "Background";
  }
  return "Unknown";
}

const char* LossReasonName(LossReason r) {
  switch (r) {
    case LossReason::QueueLimit:     return "queue_limit";
    case LossReason::RateLimit:      return "rate_limit";
    case LossReason::Coalescing:     return "coalescing";
    case LossReason::Deduplication:  return "deduplication";
    case LossReason::Aggregation:    return "aggregation";
    case LossReason::Backpressure:   return "backpressure";
  }
  return "unknown";
}

bool StormConfig::isValid() const {
  if (queue_limit == 0) return false;
  if (rate_limit_per_second == 0) return false;
  return true;
}

bool StormEvent::isValid() const {
  return !event_id.empty() && !collector.empty();
}

bool LossEvent::isValid() const {
  return !collector.empty() && events_dropped > 0;
}

std::string LossEvent::summary() const {
  return collector + " dropped=" + std::to_string(events_dropped) +
    " reason=" + std::string(LossReasonName(reason)) +
    " window=" + std::to_string(window_end_ms - window_start_ms) + "ms";
}

std::string LossEvent::action() const {
  return "collector.event_loss";
}

bool StormResult::wasAccepted() const {
  return accepted;
}

std::string StormResult::summary() const {
  if (accepted) return "accepted";
  return "dropped (" + std::string(LossReasonName(loss_reason)) + ")";
}

StormProtector::StormProtector() {}
StormProtector::StormProtector(const StormConfig& cfg) : config_(cfg) {}
StormProtector::~StormProtector() {}

void StormProtector::setLossCallback(StormCallback cb) {
  std::lock_guard<std::mutex> lock(mu_);
  loss_callback_ = std::move(cb);
}

void StormProtector::setEventCallback(EventCallback cb) {
  std::lock_guard<std::mutex> lock(mu_);
  event_callback_ = std::move(cb);
}

void StormProtector::setConfig(const StormConfig& cfg) {
  std::lock_guard<std::mutex> lock(mu_);
  config_ = cfg;
}

StormConfig StormProtector::getConfig() const {
  std::lock_guard<std::mutex> lock(mu_);
  return config_;
}

StormResult StormProtector::ingest(const StormEvent& event) {
  total_ingested_++;

  std::lock_guard<std::mutex> lock(mu_);

  auto result = evaluatePriority(event);
  if (!result.accepted) {
    states_[event.collector].pending_loss++;
    total_dropped_++;
    return result;
  }

  result = evaluateDedup(event);
  if (!result.accepted) {
    states_[event.collector].pending_loss++;
    total_dropped_++;
    return result;
  }

  result = evaluateRate(event);
  if (!result.accepted) {
    states_[event.collector].pending_loss++;
    total_dropped_++;
    return result;
  }

  result = evaluateQueue(event);
  if (!result.accepted) {
    states_[event.collector].pending_loss++;
    total_dropped_++;
    auto& cs = states_[event.collector];
    if (cs.pending_loss >= 100 || cs.pending_loss == 1) {
      cs.loss_window_start = event.timestamp_ms;
    }
    cs.loss_window_end = event.timestamp_ms;
    if (cs.pending_loss >= 100) {
      emitLoss(event.collector, cs.pending_loss, LossReason::QueueLimit, cs.loss_window_start, cs.loss_window_end);
      cs.pending_loss = 0;
    }
    return result;
  }

  auto& cs = states_[event.collector];
  cs.queue_size++;
  total_accepted_++;
  emitEvent(event);
  return result;
}

StormResult StormProtector::evaluatePriority(const StormEvent& event) {
  if (event.priority > config_.min_priority) {
    return {false, LossReason::Backpressure, "below minimum priority"};
  }
  return {true, LossReason::QueueLimit, ""};
}

StormResult StormProtector::evaluateDedup(const StormEvent& event) {
  auto& cs = states_[event.collector];
  auto now_ms = event.timestamp_ms;

  for (const auto& [eid, ts] : cs.recent_ids) {
    if (eid == event.event_id && (now_ms - ts) < static_cast<std::int64_t>(config_.dedup_window_ms)) {
      return {false, LossReason::Deduplication, "duplicate within window"};
    }
  }

  cs.recent_ids.push_back({event.event_id, now_ms});
  auto cutoff = now_ms - static_cast<std::int64_t>(config_.dedup_window_ms);
  cs.recent_ids.erase(
    std::remove_if(cs.recent_ids.begin(), cs.recent_ids.end(),
      [cutoff](const auto& p) { return p.second < cutoff; }),
    cs.recent_ids.end());

  return {true, LossReason::QueueLimit, ""};
}

StormResult StormProtector::evaluateRate(const StormEvent& event) {
  auto& cs = states_[event.collector];
  auto now_ms = event.timestamp_ms;

  if (now_ms - cs.rate_window_start > 1000) {
    cs.rate_count = 0;
    cs.rate_window_start = now_ms;
  }

  cs.rate_count++;
  if (cs.rate_count > config_.rate_limit_per_second) {
    return {false, LossReason::RateLimit, "exceeded rate limit"};
  }

  return {true, LossReason::QueueLimit, ""};
}

StormResult StormProtector::evaluateQueue(const StormEvent& event) {
  auto& cs = states_[event.collector];
  if (cs.queue_size >= config_.queue_limit) {
    return {false, LossReason::QueueLimit, "queue full"};
  }
  return {true, LossReason::QueueLimit, ""};
}

std::size_t StormProtector::totalIngested() const {
  return total_ingested_.load();
}

std::size_t StormProtector::totalAccepted() const {
  return total_accepted_.load();
}

std::size_t StormProtector::totalDropped() const {
  return total_dropped_.load();
}

std::size_t StormProtector::totalCoalesced() const {
  return total_coalesced_.load();
}

void StormProtector::flush() {
  std::lock_guard<std::mutex> lock(mu_);
  for (auto& [collector, cs] : states_) {
    if (cs.pending_loss > 0) {
      auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
      emitLoss(collector, cs.pending_loss, LossReason::QueueLimit, cs.loss_window_start, now_ms);
      cs.pending_loss = 0;
    }
  }
}

void StormProtector::reset() {
  std::lock_guard<std::mutex> lock(mu_);
  states_.clear();
}

void StormProtector::emitLoss(const std::string& collector, std::size_t count, LossReason reason,
    std::int64_t window_start, std::int64_t window_end) {
  if (loss_callback_) {
    LossEvent loss;
    loss.collector = collector;
    loss.events_dropped = count;
    loss.window_start_ms = window_start;
    loss.window_end_ms = window_end;
    loss.reason = reason;
    loss.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count();
    loss_callback_(loss);
  }
}

void StormProtector::emitEvent(const StormEvent& event) {
  if (event_callback_) {
    event_callback_(event);
  }
}

}  // namespace monix::collectors::storm
