#include "CollectorStatus.hpp"

namespace monix::collectors {

const char* CollectorLifecycleName(CollectorLifecycle s) {
  switch (s) {
    case CollectorLifecycle::Created:       return "Created";
    case CollectorLifecycle::Initializing:  return "Initializing";
    case CollectorLifecycle::Running:       return "Running";
    case CollectorLifecycle::Degraded:      return "Degraded";
    case CollectorLifecycle::Stopping:      return "Stopping";
    case CollectorLifecycle::Stopped:       return "Stopped";
    case CollectorLifecycle::Failed:        return "Failed";
  }
  return "Unknown";
}

bool CollectorStatus::canStart() const {
  return lifecycle == CollectorLifecycle::Created ||
         lifecycle == CollectorLifecycle::Stopped ||
         lifecycle == CollectorLifecycle::Failed;
}

bool CollectorStatus::canStop() const {
  return lifecycle == CollectorLifecycle::Running ||
         lifecycle == CollectorLifecycle::Degraded;
}

bool CollectorStatus::isOperational() const {
  return lifecycle == CollectorLifecycle::Running ||
         lifecycle == CollectorLifecycle::Degraded;
}

bool isValidTransition(CollectorLifecycle from, CollectorLifecycle to) {
  switch (from) {
    case CollectorLifecycle::Created:
      return to == CollectorLifecycle::Initializing;
    case CollectorLifecycle::Initializing:
      return to == CollectorLifecycle::Running ||
             to == CollectorLifecycle::Failed;
    case CollectorLifecycle::Running:
      return to == CollectorLifecycle::Degraded ||
             to == CollectorLifecycle::Stopping;
    case CollectorLifecycle::Degraded:
      return to == CollectorLifecycle::Running ||
             to == CollectorLifecycle::Stopping;
    case CollectorLifecycle::Stopping:
      return to == CollectorLifecycle::Stopped;
    case CollectorLifecycle::Stopped:
      return to == CollectorLifecycle::Initializing;
    case CollectorLifecycle::Failed:
      return to == CollectorLifecycle::Initializing;
  }
  return false;
}

}  // namespace monix::collectors
