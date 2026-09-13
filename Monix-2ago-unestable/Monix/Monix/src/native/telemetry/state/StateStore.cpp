#include "StateStore.hpp"

namespace monix::telemetry {

void StateStore::DetectChanges(const Snapshot& current, const Snapshot* prev) {
  lastChanges_.Clear();

  if (!prev) {
    entities_.UpdateProcesses(current.processes, current.collectedAtMonotonicNs, lastChanges_);
    entities_.UpdateDrivers(current.driverNames, current.collectedAtMonotonicNs, lastChanges_);
    return;
  }

  entities_.UpdateProcesses(current.processes, current.collectedAtMonotonicNs, lastChanges_);
  entities_.UpdateDrivers(current.driverNames, current.collectedAtMonotonicNs, lastChanges_);

  if (current.processHash != prev->processHash) {
    lastChanges_.AddValueChanged({}, "processHash", current.collectedAtMonotonicNs);
  }
  if (current.moduleHash != prev->moduleHash) {
    lastChanges_.AddValueChanged({}, "moduleHash", current.collectedAtMonotonicNs);
  }
}

}
