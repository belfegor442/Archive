#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace monix::collectors::snapshot {

using SnapshotId = std::uint64_t;

enum class SnapshotKind : std::uint8_t {
  Initial,
  Recovery,
  Manual
};

const char* SnapshotKindName(SnapshotKind k);

enum class SnapshotState : std::uint8_t {
  Pending,
  Running,
  Completed,
  Failed,
  Partial
};

const char* SnapshotStateName(SnapshotState s);

enum class SnapshotEventKind : std::uint8_t {
  Started,
  Completed,
  Failed
};

const char* SnapshotEventKindName(SnapshotEventKind k);
std::string SnapshotEventKindAction(SnapshotEventKind k);

enum class ObservationOrigin : std::uint8_t {
  InitialSnapshot,
  RecoverySnapshot,
  ManualSnapshot,
  Polling
};

const char* ObservationOriginName(ObservationOrigin o);

struct SnapshotMeta {
  SnapshotId id = 0;
  SnapshotKind kind = SnapshotKind::Manual;
  SnapshotState state = SnapshotState::Pending;
  std::string collector_name;
  std::string description;
  std::size_t items_expected = 0;
  std::size_t items_captured = 0;
  std::size_t items_failed = 0;
  std::int64_t started_at_ms = 0;
  std::int64_t completed_at_ms = 0;
  std::string error_message;

  bool isValid() const;
  bool isComplete() const;
  bool isFailed() const;
  bool isPartial() const;
  double progress() const;
  std::string summary() const;
  ObservationOrigin origin() const;
};

struct SnapshotEvent {
  SnapshotId id = 0;
  SnapshotEventKind event_kind = SnapshotEventKind::Started;
  SnapshotKind snapshot_kind = SnapshotKind::Manual;
  std::string collector_name;
  std::size_t items_expected = 0;
  std::size_t items_captured = 0;
  std::string error_message;
  std::int64_t timestamp_ms = 0;

  bool isValid() const;
  std::string summary() const;
};

}  // namespace monix::collectors::snapshot
