#pragma once

#include "../logging/LogEntry.hpp"
#include "../core/TextUtils.hpp"

#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

namespace monix {

struct LogManagerConfig {
  LogLevel minLevel = LogLevel::Debug;
  bool deduplicate = true;
  bool logMilliseconds = true;
  int bufferSize = 480;
  int historyCapacity = 552;
  std::wstring sessionId;
  std::wstring userId;
};

class LogManager {
 public:
  explicit LogManager(const LogManagerConfig& config);

  bool Push(std::wstring domain,
            std::wstring severity,
            std::wstring message,
            ColorRole color,
            std::wstring module = {},
            std::wstring service = {},
            std::wstring metadata = {},
            EventType eventType = EventType::TelemetrySample);

  std::vector<LogEntry> Entries() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return {entries_.begin(), entries_.end()};
  }
  SessionCounters Counters() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return counters_;
  }
  std::uint64_t NextEventId() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return nextEventId_;
  }
  LogLevel GetMinLevel() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_.minLevel;
  }

  std::vector<double> WarnHistory() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return warnHistory_;
  }
  std::vector<double> ErrHistory() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return errHistory_;
  }
  std::vector<double> CritHistory() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return critHistory_;
  }
  std::vector<double> KernelHistory() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return kernelHistory_;
  }
  std::vector<double> NetHistory() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return netHistory_;
  }

  void Clear();

  void SetMinLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_.minLevel = level;
  }
  void SetDeduplicate(bool dedup) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_.deduplicate = dedup;
  }
  void SetLogMilliseconds(bool ms) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_.logMilliseconds = ms;
  }

  TelemetryHealthManager& HealthManager() { return healthManager_; }
  const TelemetryHealthManager& HealthManager() const { return healthManager_; }

  std::function<void(const LogEntry&)> onNotification;
  std::function<void()> onSound;

  struct GlobalRateLimiter {
    int windowStartMs = 0;
    int eventsInWindow = 0;
    int droppedInfo = 0;
    int droppedWarn = 0;

    bool Allow(LogLevel level) {
      int nowMs = static_cast<int>(GetTickCount64() / 1000);
      if (nowMs - windowStartMs >= 60) {
        windowStartMs = nowMs;
        eventsInWindow = 0;
        droppedInfo = 0;
        droppedWarn = 0;
      }
      ++eventsInWindow;
      if (eventsInWindow > 30 && (level == LogLevel::Info || level == LogLevel::Debug)) {
        ++droppedInfo;
        return false;
      }
      if (eventsInWindow > 60 && level == LogLevel::Warn) {
        ++droppedWarn;
        return false;
      }
      return true;
    }
  };

  GlobalRateLimiter globalRateLimiter_;

 private:
  void UpdateCounters(LogLevel level, ColorRole color);
  void UpdateHistory(LogLevel level, ColorRole color);

  LogManagerConfig config_;
  mutable std::mutex mutex_;
  std::deque<LogEntry> entries_;
  SessionCounters counters_;
  std::uint64_t nextEventId_ = 1;
  std::uint64_t nextSequenceId_ = 1;
  std::vector<double> warnHistory_;
  std::vector<double> errHistory_;
  std::vector<double> critHistory_;
  std::vector<double> kernelHistory_;
  std::vector<double> netHistory_;
  TelemetryHealthManager healthManager_;
};

} // namespace monix
