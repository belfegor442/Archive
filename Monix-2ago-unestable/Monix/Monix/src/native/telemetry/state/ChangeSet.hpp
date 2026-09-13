#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "../contract/EntityIdentity.hpp"

namespace monix::telemetry {

struct ProcessDelta {
  double cpuDelta = 0.0;
  std::uint64_t ramDelta = 0;
  std::uint64_t ioReadDelta = 0;
  std::uint64_t ioWriteDelta = 0;
  int handleDelta = 0;
};

struct ChangeEvent {
  enum class Kind { Created, Terminated, Modified, StateChanged, ValueChanged };

  Kind kind = Kind::Modified;
  EntityIdentity entity;
  const char* attribute = nullptr;
  std::uint64_t timestampNs = 0;
  int consecutiveCount = 0;
};

struct ChangeSet {
  std::vector<ChangeEvent> events;

  bool Empty() const { return events.empty(); }
  std::size_t Count() const { return events.size(); }

  void Clear() { events.clear(); }

  void AddCreated(EntityIdentity id, const char* attr, std::uint64_t tsNs) {
    events.push_back({ChangeEvent::Kind::Created, id, attr, tsNs, 1});
  }

  void AddTerminated(EntityIdentity id, const char* attr, std::uint64_t tsNs) {
    events.push_back({ChangeEvent::Kind::Terminated, id, attr, tsNs, 1});
  }

  void AddModified(EntityIdentity id, const char* attr, std::uint64_t tsNs, int count = 1) {
    events.push_back({ChangeEvent::Kind::Modified, id, attr, tsNs, count});
  }

  void AddStateChanged(EntityIdentity id, const char* attr, std::uint64_t tsNs) {
    events.push_back({ChangeEvent::Kind::StateChanged, id, attr, tsNs, 1});
  }

  void AddValueChanged(EntityIdentity id, const char* attr, std::uint64_t tsNs) {
    events.push_back({ChangeEvent::Kind::ValueChanged, id, attr, tsNs, 1});
  }

  void Merge(const ChangeSet& other) {
    events.insert(events.end(), other.events.begin(), other.events.end());
  }
};

}
