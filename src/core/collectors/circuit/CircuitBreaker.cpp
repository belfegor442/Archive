#include "CircuitBreaker.hpp"

#include <algorithm>
#include <chrono>

namespace monix::collectors::circuit {

const char* CircuitStateName(CircuitState s) {
  switch (s) {
    case CircuitState::Closed:   return "Closed";
    case CircuitState::Open:     return "Open";
    case CircuitState::HalfOpen: return "HalfOpen";
  }
  return "Unknown";
}

bool CircuitBreakerConfig::isValid() const {
  if (failure_threshold == 0) return false;
  if (cooldown_ms <= 0) return false;
  if (half_open_max_calls == 0) return false;
  return true;
}

const char* CircuitEventKindName(CircuitEventKind k) {
  switch (k) {
    case CircuitEventKind::Opened:    return "Opened";
    case CircuitEventKind::HalfOpened: return "HalfOpened";
    case CircuitEventKind::Closed:    return "Closed";
    case CircuitEventKind::Rejected:  return "Rejected";
  }
  return "Unknown";
}

std::string CircuitEventKindAction(CircuitEventKind k) {
  switch (k) {
    case CircuitEventKind::Opened:    return "collector.circuit_open";
    case CircuitEventKind::HalfOpened: return "collector.circuit_half_open";
    case CircuitEventKind::Closed:    return "collector.circuit_closed";
    case CircuitEventKind::Rejected:  return "collector.circuit_rejected";
  }
  return "unknown";
}

bool CircuitEvent::isValid() const {
  return !collector.empty();
}

std::string CircuitEvent::summary() const {
  return collector + " " + std::string(CircuitEventKindName(kind)) +
    " state=" + std::string(CircuitStateName(state)) +
    " failures=" + std::to_string(failure_count);
}

CircuitBreaker::CircuitBreaker() {}
CircuitBreaker::CircuitBreaker(const CircuitBreakerConfig& cfg) : config_(cfg) {}
CircuitBreaker::~CircuitBreaker() {}

void CircuitBreaker::setCallback(CircuitCallback cb) {
  std::lock_guard<std::mutex> lock(mu_);
  callback_ = std::move(cb);
}

void CircuitBreaker::setConfig(const CircuitBreakerConfig& cfg) {
  std::lock_guard<std::mutex> lock(mu_);
  config_ = cfg;
}

CircuitBreakerConfig CircuitBreaker::getConfig() const {
  std::lock_guard<std::mutex> lock(mu_);
  return config_;
}

bool CircuitBreaker::tryTransitionToHalfOpen(const std::string& collector, std::int64_t now_ms) {
  auto it = circuits_.find(collector);
  if (it == circuits_.end()) return false;
  auto& cc = it->second;
  if (cc.state != CircuitState::Open) return false;
  if (now_ms - cc.opened_at_ms < config_.cooldown_ms) return false;
  cc.state = CircuitState::HalfOpen;
  cc.half_open_calls = 0;
  emit(CircuitEventKind::HalfOpened, collector, CircuitState::HalfOpen, cc.consecutive_failures);
  return true;
}

bool CircuitBreaker::allowRequest(const std::string& collector) {
  if (!config_.enabled) return true;

  std::lock_guard<std::mutex> lock(mu_);
  auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::system_clock::now().time_since_epoch()).count();

  auto& cc = circuits_[collector];

  if (cc.state == CircuitState::Open) {
    tryTransitionToHalfOpen(collector, now_ms);
  }

  if (cc.state == CircuitState::Closed) return true;

  if (cc.state == CircuitState::HalfOpen) {
    if (cc.half_open_calls < config_.half_open_max_calls) {
      cc.half_open_calls++;
      return true;
    }
    emit(CircuitEventKind::Rejected, collector, CircuitState::HalfOpen, cc.consecutive_failures);
    return false;
  }

  if (cc.state == CircuitState::Open) {
    emit(CircuitEventKind::Rejected, collector, CircuitState::Open, cc.consecutive_failures);
    return false;
  }

  return false;
}

void CircuitBreaker::recordSuccess(const std::string& collector) {
  std::lock_guard<std::mutex> lock(mu_);
  auto& cc = circuits_[collector];

  if (cc.state == CircuitState::HalfOpen) {
    cc.state = CircuitState::Closed;
    cc.consecutive_failures = 0;
    cc.half_open_calls = 0;
    emit(CircuitEventKind::Closed, collector, CircuitState::Closed, 0);
    return;
  }

  cc.consecutive_failures = 0;
}

void CircuitBreaker::recordFailure(const std::string& collector) {
  std::lock_guard<std::mutex> lock(mu_);
  auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::system_clock::now().time_since_epoch()).count();

  auto& cc = circuits_[collector];
  cc.consecutive_failures++;
  cc.last_failure_ms = now_ms;

  if (cc.state == CircuitState::HalfOpen) {
    cc.state = CircuitState::Open;
    cc.opened_at_ms = now_ms;
    emit(CircuitEventKind::Opened, collector, CircuitState::Open, cc.consecutive_failures);
    return;
  }

  if (cc.state == CircuitState::Closed && cc.consecutive_failures >= config_.failure_threshold) {
    cc.state = CircuitState::Open;
    cc.opened_at_ms = now_ms;
    emit(CircuitEventKind::Opened, collector, CircuitState::Open, cc.consecutive_failures);
  }
}

CircuitState CircuitBreaker::getState(const std::string& collector) const {
  std::lock_guard<std::mutex> lock(mu_);
  auto it = circuits_.find(collector);
  if (it == circuits_.end()) return CircuitState::Closed;
  return it->second.state;
}

std::size_t CircuitBreaker::failureCount(const std::string& collector) const {
  std::lock_guard<std::mutex> lock(mu_);
  auto it = circuits_.find(collector);
  if (it == circuits_.end()) return 0;
  return it->second.consecutive_failures;
}

std::int64_t CircuitBreaker::lastFailureMs(const std::string& collector) const {
  std::lock_guard<std::mutex> lock(mu_);
  auto it = circuits_.find(collector);
  if (it == circuits_.end()) return 0;
  return it->second.last_failure_ms;
}

bool CircuitBreaker::isOpen(const std::string& collector) const {
  return getState(collector) == CircuitState::Open;
}

bool CircuitBreaker::isHalfOpen(const std::string& collector) const {
  return getState(collector) == CircuitState::HalfOpen;
}

void CircuitBreaker::reset(const std::string& collector) {
  std::lock_guard<std::mutex> lock(mu_);
  circuits_.erase(collector);
}

void CircuitBreaker::resetAll() {
  std::lock_guard<std::mutex> lock(mu_);
  circuits_.clear();
}

std::size_t CircuitBreaker::eventsEmitted() const {
  return events_emitted_.load();
}

void CircuitBreaker::emit(CircuitEventKind kind, const std::string& collector, CircuitState state, std::size_t failure_count) {
  events_emitted_++;
  if (callback_) {
    CircuitEvent event;
    event.collector = collector;
    event.kind = kind;
    event.state = state;
    event.failure_count = failure_count;
    event.timestamp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count();
    callback_(event);
  }
}

}  // namespace monix::collectors::circuit
