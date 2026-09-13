#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>

namespace monix::collectors::health {

struct RestartPolicyConfig {
  bool auto_restart = false;
  std::size_t max_restart_attempts = 3;
  std::int64_t initial_backoff_ms = 1000;
  double backoff_multiplier = 2.0;
  std::int64_t max_backoff_ms = 30000;

  bool isValid() const;
};

enum class RestartAction : std::uint8_t {
  None,
  Restart,
  Reject
};

const char* RestartActionName(RestartAction a);

struct RestartDecision {
  RestartAction action = RestartAction::None;
  std::size_t attempt = 0;
  std::int64_t backoff_ms = 0;
  std::string reason;

  bool shouldRestart() const;
  std::string summary() const;
};

class RestartPolicy {
public:
  RestartPolicy();
  explicit RestartPolicy(const RestartPolicyConfig& cfg);
  ~RestartPolicy();

  RestartPolicy(const RestartPolicy&) = delete;
  RestartPolicy& operator=(const RestartPolicy&) = delete;

  void setConfig(const RestartPolicyConfig& cfg);
  RestartPolicyConfig getConfig() const;

  RestartDecision recordFailure(const std::string& collector);
  void recordSuccess(const std::string& collector);

  std::size_t restartCount(const std::string& collector) const;
  std::int64_t lastBackoffMs(const std::string& collector) const;
  bool isExhausted(const std::string& collector) const;

  void reset(const std::string& collector);
  void resetAll();

private:
  std::int64_t calculateBackoff(std::size_t attempt) const;

  RestartPolicyConfig config_;
  mutable std::mutex mu_;

  struct CollectorState {
    std::size_t consecutive_failures = 0;
    std::size_t total_restarts = 0;
    std::int64_t last_backoff_ms = 0;
    bool exhausted = false;
  };

  std::unordered_map<std::string, CollectorState> states_;
};

}  // namespace monix::collectors::health
