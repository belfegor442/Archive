#pragma once

#include <cstdint>

#include "ChangeSet.hpp"

namespace monix {

struct Snapshot;

namespace telemetry {

class IChangeDetector {
public:
  virtual ~IChangeDetector() = default;

  virtual const char* Name() const = 0;

  virtual void Detect(
    const Snapshot& current,
    const Snapshot* previous,
    std::uint64_t timestampNs,
    ChangeSet& out) = 0;
};

}
}