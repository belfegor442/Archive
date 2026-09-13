#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>

namespace monix::collectors::circuit {

enum class CircuitState : std::uint8_t {
  Closed,
  Open,
  HalfOpen
};

const char* CircuitStateName(CircuitState s);

struct CircuitBreakerConfig {
  std::size_t failure_threshold = 5;
  std::int64_t cooldown_ms = 1000;
  std::size_t half_open_max_calls = 1;
  bool enabled = true;

  bool isValid() const;
};

enum class CircuitEventKind : std::uint8_t {
  Opened,
  HalfOpened,
  Closed,
  Rejected
};

const char* CircuitEventKindName(CircuitEventKind k);
std::string CircuitEventKindAction(CircuitEventKind k);

struct CircuitEvent {
  std::string collector;
  CircuitEventKind kind = CircuitEventKind::Opened;
  CircuitState state = CircuitState::Closed;
  std::size_t failure_count = 0;
  std::int64_t timestamp_ms = 0;

  bool isValid() const;
  std::string summary() const;
};

using CircuitCallback = std::function<void(const CircuitEvent&)>;

class CircuitBreaker {
public:
  CircuitBreaker();
  explicit CircuitBreaker(const CircuitBreakerConfig& cfg);
  ~CircuitBreaker();

  CircuitBreaker(const CircuitBreaker&) = delete;
  CircuitBreaker& operator=(const CircuitBreaker&) = delete;

  void setCallback(CircuitCallback cb);

  void setConfig(const CircuitBreakerConfig& cfg);
  CircuitBreakerConfig getConfig() const;

  bool allowRequest(const std::string& collector);
  void recordSuccess(const std::string& collector);
  void recordFailure(const std::string& collector);

  CircuitState getState(const std::string& collector) const;
  std::size_t failureCount(const std::string& collector) const;
  std::int64_t lastFailureMs(const std::string& collector) const;
  bool isOpen(const std::string& collector) const;
  bool isHalfOpen(const std::string& collector) const;

  void reset(const std::string& collector);
  void resetAll();

  std::size_t eventsEmitted() const;

private:
  void emit(CircuitEventKind kind, const std::string& collector, CircuitState state, std::size_t failure_count);
  bool tryTransitionToHalfOpen(const std::string& collector, std::int64_t now_ms);

  CircuitCallback callback_;
  mutable std::mutex mu_;
  CircuitBreakerConfig config_;
  std::atomic<std::size_t> events_emitted_{0};

  struct CollectorCircuit {
    CircuitState state = CircuitState::Closed;
    std::size_t consecutive_failures = 0;
    std::int64_t last_failure_ms = 0;
    std::int64_t opened_at_ms = 0;
    std::size_t half_open_calls = 0;
  };

  std::unordered_map<std::string, CollectorCircuit> circuits_;
};

}  // namespace monix::collectors::circuit
