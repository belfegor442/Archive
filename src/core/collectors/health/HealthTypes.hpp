#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace monix::collectors::health {

enum class GapEventKind : std::uint8_t {
  GapDetected,
  Recovered
};

const char* GapEventKindName(GapEventKind k);
std::string GapEventKindAction(GapEventKind k);

enum class CollectorHealthState : std::uint8_t {
  Unknown,
  Started,
  Running,
  Degraded,
  Failed,
  Stopped,
  Recovered
};

const char* CollectorHealthStateName(CollectorHealthState s);

struct ObservationGap {
  std::string collector;
  std::int64_t started_at_ms = 0;
  std::int64_t ended_at_ms = 0;
  std::int64_t duration_ms = 0;
  std::string reason;

  bool isValid() const;
  bool isActive() const;
  std::string summary() const;
};

struct GapEvent {
  GapEventKind kind = GapEventKind::GapDetected;
  std::string collector;
  std::int64_t started_at_ms = 0;
  std::int64_t ended_at_ms = 0;
  std::int64_t duration_ms = 0;
  std::string reason;
  std::int64_t timestamp_ms = 0;

  bool isValid() const;
  std::string summary() const;
};

struct HealthEvent {
  std::string collector;
  CollectorHealthState state = CollectorHealthState::Unknown;
  std::string reason;
  std::int64_t timestamp_ms = 0;

  bool isValid() const;
  std::string action() const;
  std::string summary() const;
};

struct CollectorMetrics {
  std::size_t events_generated = 0;
  std::size_t events_dropped = 0;
  std::size_t errors = 0;
  std::int64_t latency_us = 0;
  double cpu_usage = 0.0;
  std::size_t memory_bytes = 0;
  std::int64_t last_success_ms = 0;
  std::size_t queue_depth = 0;
  std::size_t observation_gap_count = 0;

  bool isValid() const;
  double errorRate() const;
  double dropRate() const;
  std::string summary() const;
};

}  // namespace monix::collectors::health
