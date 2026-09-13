#pragma once

#include <cstdint>
#include <functional>

#include "../Snapshot.hpp"
#include "ChangeSet.hpp"
#include "EntityTracker.hpp"
#include "TimelineRing.hpp"

namespace monix::telemetry {

class StateStore {
public:
  void Update(Snapshot snapshot) {
    const Snapshot* prev = timeline_.Count() > 0 ? &timeline_.Current() : nullptr;
    DetectChanges(snapshot, prev);
    timeline_.Push(snapshot);
  }

  const Snapshot& Current() const { return timeline_.Current(); }
  const Snapshot* Previous() const { return timeline_.Previous(); }
  const Snapshot* At(std::size_t offset) const { return timeline_.At(offset); }
  std::size_t HistorySize() const { return timeline_.Count(); }

  const ChangeSet& LastChanges() const { return lastChanges_; }
  const EntityTracker& Entities() const { return entities_; }
  EntityTracker& MutableEntities() { return entities_; }

  void Clear() {
    timeline_.Clear();
    lastChanges_.Clear();
    entities_ = EntityTracker{};
  }

private:
  void DetectChanges(const Snapshot& current, const Snapshot* prev);

  TimelineRing timeline_;
  ChangeSet lastChanges_;
  EntityTracker entities_;
};

}
