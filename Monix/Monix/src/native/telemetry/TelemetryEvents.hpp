#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <chrono>
#include <algorithm>

#include "../core/Types.hpp"

namespace monix {

enum class EventType : std::uint8_t {
  TelemetrySample = 0,
  StateChange = 1,
  Anomaly = 2,
  Warning = 3,
  Error = 4,
  Debug = 5,
  Diagnostic = 6,
  LifecycleEvent = 7,
};

inline const wchar_t* EventTypeName(EventType t) {
  switch (t) {
    case EventType::TelemetrySample:  return L"TELEMETRY";
    case EventType::StateChange:      return L"STATE_CHANGE";
    case EventType::Anomaly:          return L"ANOMALY";
    case EventType::Warning:          return L"WARNING";
    case EventType::Error:            return L"ERROR";
    case EventType::Debug:            return L"DEBUG";
    case EventType::Diagnostic:       return L"DIAGNOSTIC";
    case EventType::LifecycleEvent:   return L"LIFECYCLE";
  }
  return L"UNKNOWN";
}

enum class CollectorState : std::uint8_t {
  Healthy = 0,
  Degraded = 1,
  Unavailable = 2,
  Error = 3,
  Stale = 4,
};

inline const wchar_t* CollectorStateName(CollectorState s) {
  switch (s) {
    case CollectorState::Healthy:     return L"HEALTHY";
    case CollectorState::Degraded:    return L"DEGRADED";
    case CollectorState::Unavailable: return L"UNAVAILABLE";
    case CollectorState::Error:       return L"ERROR";
    case CollectorState::Stale:       return L"STALE";
  }
  return L"UNKNOWN";
}

struct TelemetrySource {
  std::wstring name;
  CollectorState state = CollectorState::Healthy;
  std::uint64_t lastSampleTimeNs = 0;
  std::uint64_t staleAfterNs = 5000000000ULL;
  int consecutiveFailures = 0;
  int totalSamples = 0;
  int failedSamples = 0;
  std::wstring lastError;

  bool IsStale(std::uint64_t nowNs) const {
    if (lastSampleTimeNs == 0) return false;
    return (nowNs - lastSampleTimeNs) > staleAfterNs;
  }

  void RecordSuccess(std::uint64_t nowNs) {
    lastSampleTimeNs = nowNs;
    consecutiveFailures = 0;
    ++totalSamples;
    if (state == CollectorState::Stale || state == CollectorState::Error) {
      state = CollectorState::Healthy;
    }
  }

  void RecordFailure(std::uint64_t nowNs, const std::wstring& error = {}) {
    lastSampleTimeNs = nowNs;
    ++consecutiveFailures;
    ++failedSamples;
    lastError = error;
    if (consecutiveFailures >= 5) {
      state = CollectorState::Error;
    } else if (consecutiveFailures >= 3) {
      state = CollectorState::Degraded;
    }
  }
};

struct SubsystemRateLimiter {
  int maxEventsPerTick = 20;
  int eventsThisTick = 0;
  int totalSuppressed = 0;

  bool Allow() {
    if (eventsThisTick >= maxEventsPerTick) {
      ++totalSuppressed;
      return false;
    }
    ++eventsThisTick;
    return true;
  }

  void ResetTick() {
    eventsThisTick = 0;
  }
};

struct CausalGroup {
  std::wstring rootCause;
  std::vector<std::wstring> relatedEvents;
  std::uint64_t firstEventNs = 0;
  std::uint64_t lastEventNs = 0;
  int eventCount = 0;
  bool emitted = false;

  void AddEvent(const std::wstring& event, std::uint64_t nowNs) {
    if (firstEventNs == 0) firstEventNs = nowNs;
    lastEventNs = nowNs;
    ++eventCount;
    if (std::find(relatedEvents.begin(), relatedEvents.end(), event) == relatedEvents.end()) {
      relatedEvents.push_back(event);
    }
  }

  bool ShouldEmit(std::uint64_t nowNs) const {
    if (emitted) return false;
    if (eventCount >= 2 && (nowNs - firstEventNs) > 2000000000ULL) {
      return true;
    }
    return eventCount >= 3;
  }
};

struct HysteresisState {
  double enterThreshold = 0.0;
  double exitThreshold = 0.0;
  bool currentlyActive = false;
  int activeSampleCount = 0;
  int inactiveSampleCount = 0;
  int minActiveSamples = 3;
  int minInactiveSamples = 3;

  bool Update(double value) {
    if (!currentlyActive) {
      if (value >= enterThreshold) {
        ++activeSampleCount;
        inactiveSampleCount = 0;
        if (activeSampleCount >= minActiveSamples) {
          currentlyActive = true;
          return true;
        }
      } else {
        activeSampleCount = 0;
      }
    } else {
      if (value <= exitThreshold) {
        ++inactiveSampleCount;
        activeSampleCount = 0;
        if (inactiveSampleCount >= minInactiveSamples) {
          currentlyActive = false;
          return false;
        }
      } else {
        inactiveSampleCount = 0;
      }
    }
    return currentlyActive;
  }
};

class TelemetryHealthManager {
 public:
  TelemetrySource& GetSource(const std::wstring& name) {
    return sources_[name];
  }

  const TelemetrySource& GetSource(const std::wstring& name) const {
    auto it = sources_.find(name);
    if (it != sources_.end()) return it->second;
    static const TelemetrySource empty;
    return empty;
  }

  void CheckStaleness(std::uint64_t nowNs) {
    for (auto& [name, source] : sources_) {
      if (source.state != CollectorState::Error && source.IsStale(nowNs)) {
        source.state = CollectorState::Stale;
      }
    }
  }

  std::vector<std::pair<std::wstring, CollectorState>> GetUnhealthy() const {
    std::vector<std::pair<std::wstring, CollectorState>> result;
    for (const auto& [name, source] : sources_) {
      if (source.state != CollectorState::Healthy) {
        result.emplace_back(name, source.state);
      }
    }
    return result;
  }

  SubsystemRateLimiter& GetRateLimiter(const std::wstring& domain) {
    return rateLimiters_[domain];
  }

  void ResetAllTick() {
    for (auto& [name, limiter] : rateLimiters_) {
      limiter.ResetTick();
    }
  }

  CausalGroup& GetCausalGroup(const std::wstring& key) {
    return causalGroups_[key];
  }

  void CleanupOldGroups(std::uint64_t nowNs) {
    for (auto it = causalGroups_.begin(); it != causalGroups_.end();) {
      if ((nowNs - it->second.lastEventNs) > 30000000000ULL) {
        it = causalGroups_.erase(it);
      } else {
        ++it;
      }
    }
  }

 private:
  std::unordered_map<std::wstring, TelemetrySource> sources_;
  std::unordered_map<std::wstring, SubsystemRateLimiter> rateLimiters_;
  std::unordered_map<std::wstring, CausalGroup> causalGroups_;
};

} // namespace monix
