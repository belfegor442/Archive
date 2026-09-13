#pragma once

#include <cstdint>
#include <memory>

#include "IChangeDetector.hpp"
#include "EntityTracker.hpp"

namespace monix::telemetry {

class ProcessChangeDetector : public IChangeDetector {
public:
  ProcessChangeDetector() : tracker_(std::make_unique<EntityTracker>()) {}

  const char* Name() const override { return "ProcessChangeDetector"; }

  void Detect(
    const Snapshot& current,
    const Snapshot* previous,
    std::uint64_t timestampNs,
    ChangeSet& out) override
  {
    tracker_->UpdateProcesses(current.processes, timestampNs, out);
  }

  const EntityTracker& Tracker() const { return *tracker_; }
  EntityTracker& Tracker() { return *tracker_; }

private:
  std::unique_ptr<EntityTracker> tracker_;
};

}