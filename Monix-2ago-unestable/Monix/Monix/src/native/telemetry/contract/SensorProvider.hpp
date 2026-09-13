#pragma once

#include <cstdint>

#include "SensorState.hpp"

namespace monix {

struct Snapshot;

namespace telemetry {

class ISensorProvider {
public:
  virtual ~ISensorProvider() = default;
  virtual const char* Name() const = 0;
  virtual const char* Source() const = 0;
  virtual void Poll(Snapshot& snapshot, const Snapshot* previous) = 0;
  virtual SensorState State() const = 0;
  virtual std::uint64_t LastUpdateNs() const = 0;
  virtual std::uint64_t Sequence() const = 0;
};

}
}
