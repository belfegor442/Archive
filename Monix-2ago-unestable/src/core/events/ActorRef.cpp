#include "ActorRef.hpp"

#include <cstring>

namespace monix::events {

const char* ActorKindName(ActorKind k) {
  switch (k) {
    case ActorKind::User:       return "user";
    case ActorKind::Process:    return "process";
    case ActorKind::System:     return "system";
    case ActorKind::Service:    return "service";
    case ActorKind::Script:     return "script";
    case ActorKind::Automation: return "automation";
    case ActorKind::Remote:     return "remote";
    case ActorKind::Device:     return "device";
    case ActorKind::Unknown:    return "unknown";
  }
  return "unknown";
}

ActorKind ActorKindFromString(const char* s) {
  if (std::strcmp(s, "user") == 0)       return ActorKind::User;
  if (std::strcmp(s, "process") == 0)    return ActorKind::Process;
  if (std::strcmp(s, "system") == 0)     return ActorKind::System;
  if (std::strcmp(s, "service") == 0)    return ActorKind::Service;
  if (std::strcmp(s, "script") == 0)     return ActorKind::Script;
  if (std::strcmp(s, "automation") == 0) return ActorKind::Automation;
  if (std::strcmp(s, "remote") == 0)     return ActorKind::Remote;
  if (std::strcmp(s, "device") == 0)     return ActorKind::Device;
  return ActorKind::Unknown;
}

}  // namespace monix::events
