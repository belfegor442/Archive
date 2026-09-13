#pragma once

#include "CollectorInfo.hpp"
#include "CollectorCapabilities.hpp"
#include "CollectorStatus.hpp"
#include "CollectorConfig.hpp"
#include "../events/Event.hpp"

#include <memory>
#include <functional>

namespace monix::collectors {

using EventCallback = std::function<void(events::Event)>;

class ICollector {
public:
  virtual ~ICollector() = default;

  virtual const CollectorId& id() const = 0;
  virtual const CollectorInfo& info() const = 0;
  virtual CollectorStatus status() const = 0;
  virtual CollectorCapability capabilities() const = 0;

  virtual bool start(const CollectorConfig& config) = 0;
  virtual bool stop() = 0;

  virtual void setEventCallback(EventCallback callback) = 0;
};

}  // namespace monix::collectors
