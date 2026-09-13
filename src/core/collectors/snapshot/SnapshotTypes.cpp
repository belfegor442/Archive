#include "SnapshotTypes.hpp"

namespace monix::collectors::snapshot {

const char* SnapshotKindName(SnapshotKind k) {
  switch (k) {
    case SnapshotKind::Initial:  return "Initial";
    case SnapshotKind::Recovery: return "Recovery";
    case SnapshotKind::Manual:   return "Manual";
  }
  return "Unknown";
}

const char* SnapshotStateName(SnapshotState s) {
  switch (s) {
    case SnapshotState::Pending:   return "Pending";
    case SnapshotState::Running:   return "Running";
    case SnapshotState::Completed: return "Completed";
    case SnapshotState::Failed:    return "Failed";
    case SnapshotState::Partial:   return "Partial";
  }
  return "Unknown";
}

const char* SnapshotEventKindName(SnapshotEventKind k) {
  switch (k) {
    case SnapshotEventKind::Started:   return "Started";
    case SnapshotEventKind::Completed: return "Completed";
    case SnapshotEventKind::Failed:    return "Failed";
  }
  return "Unknown";
}

std::string SnapshotEventKindAction(SnapshotEventKind k) {
  switch (k) {
    case SnapshotEventKind::Started:   return "snapshot.started";
    case SnapshotEventKind::Completed: return "snapshot.completed";
    case SnapshotEventKind::Failed:    return "snapshot.failed";
  }
  return "unknown";
}

const char* ObservationOriginName(ObservationOrigin o) {
  switch (o) {
    case ObservationOrigin::InitialSnapshot:  return "InitialSnapshot";
    case ObservationOrigin::RecoverySnapshot: return "RecoverySnapshot";
    case ObservationOrigin::ManualSnapshot:   return "ManualSnapshot";
    case ObservationOrigin::Polling:          return "Polling";
  }
  return "Unknown";
}

bool SnapshotMeta::isValid() const {
  return !collector_name.empty();
}

bool SnapshotMeta::isComplete() const {
  return state == SnapshotState::Completed;
}

bool SnapshotMeta::isFailed() const {
  return state == SnapshotState::Failed;
}

bool SnapshotMeta::isPartial() const {
  return state == SnapshotState::Partial;
}

double SnapshotMeta::progress() const {
  if (items_expected == 0) return 0.0;
  return static_cast<double>(items_captured) / static_cast<double>(items_expected);
}

std::string SnapshotMeta::summary() const {
  std::string result = collector_name + " [" + std::string(SnapshotKindName(kind)) + "]";
  result += " " + std::string(SnapshotStateName(state));
  result += " " + std::to_string(items_captured) + "/" + std::to_string(items_expected);
  if (!error_message.empty()) result += " (" + error_message + ")";
  return result;
}

ObservationOrigin SnapshotMeta::origin() const {
  switch (kind) {
    case SnapshotKind::Initial:  return ObservationOrigin::InitialSnapshot;
    case SnapshotKind::Recovery: return ObservationOrigin::RecoverySnapshot;
    case SnapshotKind::Manual:   return ObservationOrigin::ManualSnapshot;
  }
  return ObservationOrigin::ManualSnapshot;
}

bool SnapshotEvent::isValid() const {
  return !collector_name.empty();
}

std::string SnapshotEvent::summary() const {
  std::string result = std::string(SnapshotEventKindName(event_kind)) + " " + collector_name;
  result += " [" + std::string(SnapshotKindName(snapshot_kind)) + "]";
  if (items_expected > 0) {
    result += " " + std::to_string(items_captured) + "/" + std::to_string(items_expected);
  }
  if (!error_message.empty()) result += " (" + error_message + ")";
  return result;
}

}  // namespace monix::collectors::snapshot
