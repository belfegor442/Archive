#include "CollectorErrors.hpp"

namespace monix::collectors {

const char* CollectorErrorCodeName(CollectorErrorCode c) {
  switch (c) {
    case CollectorErrorCode::None:                return "None";
    case CollectorErrorCode::AlreadyRegistered:   return "AlreadyRegistered";
    case CollectorErrorCode::NotRegistered:       return "NotRegistered";
    case CollectorErrorCode::AlreadyRunning:      return "AlreadyRunning";
    case CollectorErrorCode::NotRunning:          return "NotRunning";
    case CollectorErrorCode::StartFailed:         return "StartFailed";
    case CollectorErrorCode::StopFailed:          return "StopFailed";
    case CollectorErrorCode::InvalidTransition:   return "InvalidTransition";
    case CollectorErrorCode::InvalidConfig:       return "InvalidConfig";
    case CollectorErrorCode::ResourceBusy:        return "ResourceBusy";
    case CollectorErrorCode::DependencyMissing:   return "DependencyMissing";
    case CollectorErrorCode::InternalError:       return "InternalError";
  }
  return "Unknown";
}

}  // namespace monix::collectors
